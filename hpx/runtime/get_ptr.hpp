//  Copyright (c) 2007-2017 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/// \file get_ptr.hpp

#if !defined(HPX_RUNTIME_GET_PTR_SEP_18_2013_0622PM)
#define HPX_RUNTIME_GET_PTR_SEP_18_2013_0622PM

#include <hpx/config.hpp>
#include <hpx/runtime_fwd.hpp>
#include <hpx/runtime/agas/gva.hpp>
#include <hpx/runtime/components/client_base.hpp>
#include <hpx/runtime/components/component_type.hpp>
#include <hpx/runtime/get_lva.hpp>
#include <hpx/runtime/naming/address.hpp>
#include <hpx/runtime/naming/name.hpp>
#include <hpx/throw_exception.hpp>
#include <hpx/traits/component_type_is_compatible.hpp>
#include <hpx/util/assert.hpp>

#include <memory>

///////////////////////////////////////////////////////////////////////////////
namespace hpx
{
    /// \cond NOINTERNAL
    namespace detail
    {
        ///////////////////////////////////////////////////////////////////////
        struct get_ptr_deleter
        {
            get_ptr_deleter(naming::id_type const& id) : id_(id) {}

            template <typename Component>
            void operator()(Component* p)
            {
                id_ = naming::invalid_id;       // release component
                p->unpin();
            }

            naming::id_type id_;                // holds component alive
        };

        struct get_ptr_for_migration_deleter
        {
            get_ptr_for_migration_deleter(naming::id_type const& id)
              : id_(id)
            {}

            template <typename Component>
            void operator()(Component* p)
            {
                bool was_migrated = p->unpin();

                if (was_migrated)
                {
                    components::component_type type =
                        components::get_component_type<Component>();
                    components::deleter(type)(id_.get_gid(),
                        naming::address(hpx::get_locality(), type, p));
                }
                id_ = naming::invalid_id;       // release credits
            }

            naming::id_type id_;                // holds component alive
        };

        template <typename Component, typename Deleter>
        std::shared_ptr<Component>
        get_ptr_postproc(naming::address const& addr, naming::id_type const& id)
        {
            if (get_locality() != addr.locality_)
            {
                HPX_THROW_EXCEPTION(bad_parameter,
                    "hpx::get_ptr_postproc<Component, Deleter>",
                    "the given component id does not belong to a local object");
                return std::shared_ptr<Component>();
            }

            if (!traits::component_type_is_compatible<Component>::call(addr))
            {
                HPX_THROW_EXCEPTION(bad_component_type,
                    "hpx::get_ptr_postproc<Component, Deleter>",
                    "requested component type does not match the given component id");
                return std::shared_ptr<Component>();
            }

            Component* p = get_lva<Component>::call(addr.address_);
            std::shared_ptr<Component> ptr(p, Deleter(id));

            ptr->pin();     // the shared_ptr pins the component
            return ptr;
        }

        ///////////////////////////////////////////////////////////////////////
        // This is similar to get_ptr<> below, except that the shared_ptr will
        // delete the local instance when it goes out of scope.
        template <typename Component>
        std::shared_ptr<Component>
        get_ptr_for_migration(naming::address const& addr,
            naming::id_type const& id)
        {
            return get_ptr_postproc<Component, get_ptr_for_migration_deleter>(
                addr, id);
        }
    }
    /// \endcond

    /// \brief Returns a pointer to the underlying memory of a component
    ///
    /// The function hpx::get_ptr can be used to extract the pointer to
    /// the underlying memory of a given component.
    ///
    /// \param id  [in] The global id of the component for which the pointer
    ///            to the underlying memory should be retrieved.
    ///
    /// \tparam    The only template parameter has to be the type of the
    ///            server side component.
    ///
    /// \returns   This function returns a pointer to the underlying memory for
    ///            the component instance with the given \a id.
    ///
    /// \note      This function will successfully return the requested result
    ///            only if the given component is currently located on the
    ///            calling locality. Otherwise the function will raise an
    ///            error.
    ///
    /// \note      The component instance the returned pointer refers to can
    ///            not be migrated as long as there is at least one copy of the
    ///            returned shared_ptr alive.
    ///
    template <typename Component>
    std::shared_ptr<Component>
    get_ptr(naming::id_type const& id)
    {
        return detail::get_ptr_postproc<Component, detail::get_ptr_deleter>(
            agas::resolve(id), id);
    }

    /// \brief Returns the pointer to the underlying memory of a component
    ///
    /// The function hpx::get_ptr can be used to extract the pointer to
    /// the underlying memory of a given component.
    ///
    /// \param c   [in] A client side representation of the component for which
    ///            the pointer to the underlying memory should be retrieved.
    ///
    /// \returns   This function returns a pointer to the underlying memory for
    ///            the component instance with the given \a id.
    ///
    /// \note      This function will successfully return the requested result
    ///            only if the given component is currently located on the
    ///            calling locality. Otherwise the function will raise an
    ///            error.
    ///
    /// \note      The component instance the returned pointer refers to can
    ///            not be migrated as long as there is at least one copy of the
    ///            returned shared_ptr alive.
    ///
    template <typename Derived, typename Stub>
    std::shared_ptr<
        typename components::client_base<Derived, Stub>::server_component_type
    >
    get_ptr(components::client_base<Derived, Stub> const& c)
    {
        typedef typename components::client_base<
                Derived, Stub
            >::server_component_type component_type;

        return get_ptr<component_type>(c.get_id());
    }
}

#endif
