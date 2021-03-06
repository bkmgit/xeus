/***************************************************************************
* Copyright (c) 2016, Johan Mabille and Sylvain Corlay                     *
*                                                                          *
* Distributed under the terms of the BSD 3-Clause License.                 *
*                                                                          *
* The full license is in the file LICENSE, distributed with this software. *
****************************************************************************/

#ifndef XCOMM_HPP
#define XCOMM_HPP

#include <functional>
#include <map>
#include <string>
#include <utility>

#include "xguid.hpp"
#include "xmessage.hpp"

namespace xeus
{

    /***********************
     * xtarget declaration *
     ***********************/

    class xcomm;
    class xcomm_manager;

    /**
     * @class xtarget
     * @brief Comm target.
     */
    class XEUS_API xtarget
    {
    public:

        using function_type = std::function<void(xcomm&&, const xmessage&)>;

        xtarget();
        xtarget(const std::string& name, const function_type& callback, xcomm_manager* manager);

        std::string& name() & noexcept;
        const std::string& name() const & noexcept;
        std::string name() const && noexcept;

        void operator()(xcomm&& comm, const xmessage& request) const;

        void publish_message(const std::string&, xjson, xjson) const;

        void register_comm(xguid, xcomm*) const;
        void unregister_comm(xguid) const;

    private:

        std::string m_name;
        function_type m_callback;
        xcomm_manager* p_manager;
    };

    /*********************
     * xcomm declaration *
     *********************/

    /**
     * @class xcomm
     * @brief Comm object.
     *
     */
    class XEUS_API xcomm
    {
    public:

        using handler_type = std::function<void(const xmessage&)>;

        xcomm() = delete;
        ~xcomm();
        xcomm(xcomm&&);
        xcomm(const xcomm&);
        xcomm(xtarget* target);
        xcomm(xtarget* target, xguid id);

        xcomm& operator=(xcomm&&);
        xcomm& operator=(const xcomm&);

        void open(xjson metadata, xjson data);
        void close(xjson metadata, xjson data);
        void send(xjson metadata, xjson data) const;

        xtarget& target() noexcept;
        const xtarget& target() const noexcept;

        void handle_message(const xmessage& request);
        void handle_close(const xmessage& request);

        xguid id() const noexcept;

        template <class T>
        void on_message(T&& handler);
        template <class T>
        void on_close(T&& handler);

    private:

        friend class xcomm_manager;

        /**
         * Send comm message on iopub. Compose iopub message with specified data
         * and rebinds to target's publish_message
         * {
         *    "comm_id": m_id,
         *    "target_name": specified target name,
         *    "data": specified data
         * }
         */
        void send_comm_message(const std::string& msg_type, xjson metadata, xjson data) const;
        void send_comm_message(const std::string& msg_type, xjson metadata, xjson data, const std::string& target_name) const;

        handler_type m_close_handler;
        handler_type m_message_handler;
        xtarget* p_target;
        xguid m_id;
        bool m_moved_from;
    };

    /*****************************
     * xcomm_manager declaration *
     *****************************/

    class xkernel_core;

    /**
     * @class xcomm_manager
     * @brief Manager and registry for comms and comm targets in the kernel.
     */
    class XEUS_API xcomm_manager
    {
    public:

        xcomm_manager(xkernel_core* kernel = nullptr);

        using target_function_type = xtarget::function_type;

        void register_comm_target(const std::string& target_name, const target_function_type& callback);
        void unregister_comm_target(const std::string& target_name);

        void comm_open(const xmessage& request);
        void comm_close(const xmessage& request);
        void comm_msg(const xmessage& request);

        std::map<xguid, xcomm*>& comms() & noexcept;
        const std::map<xguid, xcomm*>& comms() const & noexcept;
        std::map<xguid, xcomm*> comms() const && noexcept;

        xtarget* target(const std::string& target_name);

    private:

        friend class xtarget;

        void register_comm(xguid, xcomm*);
        void unregister_comm(xguid);

        xjson get_metadata() const;

        std::map<xguid, xcomm*> m_comms;
        std::map<std::string, xtarget> m_targets;
        xkernel_core* p_kernel;
    };

    /**************************
     * xtarget implementation *
     **************************/

    inline xtarget::xtarget()
        : m_name(), m_callback(), p_manager(nullptr)
    {
    }

    inline xtarget::xtarget(const std::string& name, const function_type& callback, xcomm_manager* manager)
        : m_name(name), m_callback(callback), p_manager(manager)
    {
    }

    inline std::string& xtarget::name() & noexcept
    {
        return m_name;
    }

    inline const std::string& xtarget::name() const & noexcept
    {
        return m_name;
    }

    inline std::string xtarget::name() const && noexcept
    {
        return m_name;
    }

    inline void xtarget::operator()(xcomm&& comm, const xmessage& message) const
    {
        return m_callback(std::move(comm), message);
    }

    inline void xtarget::register_comm(xguid id, xcomm* comm) const
    {
        p_manager->register_comm(id, comm);
    }

    inline void xtarget::unregister_comm(xguid id) const
    {
        p_manager->unregister_comm(id);
    }

    /************************
     * xcomm implementation *
     ************************/

    inline xtarget& xcomm::target() noexcept
    {
        return *p_target;
    }

    inline const xtarget& xcomm::target() const noexcept
    {
        return *p_target;
    }

    inline void xcomm::handle_close(const xmessage& message)
    {
        if (m_close_handler)
        {
            m_close_handler(message);
        }
    }

    inline void xcomm::handle_message(const xmessage& message)
    {
        if (m_message_handler)
        {
            m_message_handler(message);
        }
    }

    inline void xcomm::send_comm_message(const std::string& msg_type, xjson metadata, xjson data) const
    {
        xjson content;
        content["comm_id"] = m_id;
        content["data"] = std::move(data);
        target().publish_message(msg_type, std::move(metadata), std::move(content));
    }

    inline void xcomm::send_comm_message(const std::string& msg_type, xjson metadata, xjson data,
                                         const std::string& target_name) const
    {
        xjson content;
        content["comm_id"] = m_id;
        content["target_name"] = target_name;
        content["data"] = std::move(data);
        target().publish_message(msg_type, std::move(metadata), std::move(content));
    }

    inline xcomm::xcomm(xcomm&& comm)
        : m_close_handler(std::move(comm.m_close_handler)),
          m_message_handler(std::move(comm.m_message_handler)),
          p_target(std::move(comm.p_target)),
          m_id(std::move(comm.m_id)),
          m_moved_from(false)
    {
        comm.m_moved_from = true;
        p_target->register_comm(m_id, this); // Replacing the address of the moved comm with `this`.
    }

    inline xcomm::xcomm(const xcomm& comm)
        : p_target(comm.p_target), m_id(xeus::new_xguid()), m_moved_from(false)
    {
        p_target->register_comm(m_id, this);
    }

    inline xcomm& xcomm::operator=(xcomm&& comm)
    {
        m_close_handler = std::move(comm.m_close_handler);
        m_message_handler = std::move(comm.m_message_handler);
        p_target = std::move(comm.p_target);
        p_target->unregister_comm(m_id);
        m_id = std::move(comm.m_id);
        m_moved_from = false;
        comm.m_moved_from = true;
        p_target->register_comm(m_id, this); // Replacing the address of the moved comm with `this`.
        return *this;
    }

    inline xcomm& xcomm::operator=(const xcomm& comm)
    {
        p_target = comm.p_target;
        p_target->unregister_comm(m_id);
        m_id = new_xguid();
        m_moved_from = false;
        p_target->register_comm(m_id, this);
        return *this;
    }

    inline xcomm::xcomm(xtarget* target)
        : p_target(target), m_id(xeus::new_xguid())
    {
        p_target->register_comm(m_id, this);
    }

    inline xcomm::xcomm(xtarget* target, xguid id)
        : p_target(target), m_id(id)
    {
        p_target->register_comm(m_id, this);
    }

    inline xcomm::~xcomm()
    {
        if (!m_moved_from)
        {
            p_target->unregister_comm(m_id);
        }
    }

    inline void xcomm::open(xjson metadata, xjson data)
    {
        send_comm_message("comm_open", std::move(metadata), std::move(data), p_target->name());
    }

    inline void xcomm::close(xjson metadata, xjson data)
    {
        send_comm_message("comm_close", std::move(metadata), std::move(data));
    }

    inline void xcomm::send(xjson metadata, xjson data) const
    {
        send_comm_message("comm_msg", std::move(metadata), std::move(data));
    }

    inline xguid xcomm::id() const noexcept
    {
        return m_id;
    }

    template <class T>
    inline void xcomm::on_message(T&& handler)
    {
        m_message_handler = std::forward<T>(handler);
    }

    template <class T>
    inline void xcomm::on_close(T&& handler)
    {
        m_close_handler = std::forward<T>(handler);
    }

    /********************************
     * xcomm_manager implementation *
     ********************************/

    inline xtarget* xcomm_manager::target(const std::string& target_name)
    {
        return &m_targets[target_name];
    }

    inline std::map<xguid, xcomm*>& xcomm_manager::comms() & noexcept
    {
        return m_comms;
    }

    inline const std::map<xguid, xcomm*>& xcomm_manager::comms() const & noexcept
    {
        return m_comms;
    }

    inline std::map<xguid, xcomm*> xcomm_manager::comms() const && noexcept
    {
        return m_comms;
    }
}

#endif
