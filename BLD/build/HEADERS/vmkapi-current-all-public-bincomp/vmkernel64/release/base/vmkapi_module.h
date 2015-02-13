/* **********************************************************
 * Copyright 1998 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Module                                                         */ /**
 * \defgroup Module Kernel Module Management
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_MODULE_H_
#define _VMKAPI_MODULE_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

#include "vmkapi_module_int.h"
#include "vmkapi_module_ns.h"

/**
 * \brief Module stack element.
 */
typedef struct vmk_ModInfoStack {
   /** \brief Module ID. */
   vmk_ModuleID modID;

   /** \brief Module function called. */
   void     *mod_fn;

   /** \brief Return address of caller. */
   void     *pushRA;

   /** \brief Next module stack element. */
   struct vmk_ModInfoStack *oldStack;
} vmk_ModInfoStack;

/**
 * \brief Guaranteed invalid module ID.
 */
#define VMK_INVALID_MODULE_ID    ((vmk_uint32)-1)

/**
 * \brief Module ID for vmkernel itself.
 */
#define VMK_VMKERNEL_MODULE_ID   0

/**
 * \brief VMware proprietary code
 */
#define VMK_MODULE_LICENSE_VMWARE	"VMware"
/**
 * \brief GPLv2
 */
#define VMK_MODULE_LICENSE_GPLV2	"GPLv2"
/**
 * \brief BSD compatibile license
 */
#define VMK_MODULE_LICENSE_BSD		"BSD"

/*
 ***********************************************************************
 * VMK_MODPARAM_NAMED --                                          */ /**
 *
 * \ingroup Module
 *
 * \brief Define a parameter set by the user during module load.
 *
 * \param[in] name   Name of the parameter.
 * \param[in] var    Name of variable to store parameter value.
 * \param[in] type   Type of the variable.
 * \param[in] desc   String describing the variable.
 *
 ***********************************************************************
 */
#define VMK_MODPARAM_NAMED(name, var, type, desc)	\
   __VMK_MODPARAM_NAMED(name, var, type);		\
   __VMK_MODPARAM_DESC(name, desc)

/*
 ***********************************************************************
 * VMK_MODPARAM --                                                */ /**
 *
 * \ingroup Module
 * \brief Define a parameter set by the user during module load.
 *
 * \note This macro relies on having a variable with the same name
 *       as the parameter.  If your variable has a different name
 *       than the parameter name, use VMK_MODPARAM_NAMED.
 *
 * \param[in] name   Name of the parameter and variable.
 * \param[in] type   Type of the variable.
 * \param[in] desc   String describing the variable.
 *
 ***********************************************************************
 */
#define VMK_MODPARAM(name, type, desc) \
   VMK_MODPARAM_NAMED(name, name, type, desc)

/*
 ***********************************************************************
 * VMK_MODPARAM_ARRAY_NAMED --                                    */ /**
 *
 * \ingroup Module
 *
 * \brief Define an array parameter that can be set by the user during
 *        module load.
 *
 * \param[in] name   Name of parameter.
 * \param[in] var    Name of array variable.
 * \param[in] type   Type of array elements.
 * \param[in] nump   Variable to store count of set elements.
 * \param[in] desc   String describing the variable.
 *
 ***********************************************************************
 */
#define VMK_MODPARAM_ARRAY_NAMED(name, var, type, nump, desc)	\
  __VMK_MODPARAM_ARRAY_NAMED(name, var, type, nump);	        \
  __VMK_MODPARAM_DESC(name, desc)

/*
 ***********************************************************************
 * VMK_MODPARAM_ARRAY --                                          */ /**
 *
 * \ingroup Module
 *
 * \brief Define an array parameter that can be set by the user during
 *        module load.
 *
 * \note This macro relies on having a variable with the same name
 *       as the parameter. If your variable has a different name
 *       than the parameter name, use VMK_MODPARAM_NAMED.
 *
 * \param[in] name   Name of parameter and variable.
 * \param[in] type   Type of array elements.
 * \param[in] nump   Variable to store count of set elements.
 * \param[in] desc   String describing the variable.
 *
 ***********************************************************************
 */
#define VMK_MODPARAM_ARRAY(name, type, nump, desc) \
     VMK_MODPARAM_ARRAY_NAMED(name, name, type, nump, desc)

/*
 ***********************************************************************
 * VMK_MODPARAM_STRING_NAMED --                                   */ /**
 *
 * \ingroup Module
 * \brief Define an string parameter that can be set by the user
 *        during module load.
 *
 * \note This creates a copy of the string; your variable must be an
 *       array of sufficient size to hold the copy.  If you do not
 *       need to modify the string consider using a charp type.
 *
 * \param[in] name      Name of parameter.
 * \param[in] string    Variable name for the string copy.
 * \param[in] len       Maximum length of string.
 * \param[in] desc      String describing the variable.
 *
 ***********************************************************************
 */
#define VMK_MODPARAM_STRING_NAMED(name, string, len, desc)	\
   __VMK_MODPARAM_STRING_NAMED(name, string, len);		\
   __VMK_MODPARAM_DESC(name, desc)

/*
 ***********************************************************************
 * VMK_MODPARAM_STRING --                                         */ /**
 *
 * \ingroup Module
 * \brief Define an string parameter that can be set by the user
 *        during module load.
 *
 * \note This creates a copy of the string; your variable must be an
 *       array of sufficient size to hold the copy.  If you do not
 *       need to modify the string consider using a charp type.
 *
 * \param[in] name   Name of parameter and char array variable.
 * \param[in] len    Maximum length of string.
 * \param[in] desc   String describing the variable.
 *
 ***********************************************************************
 */
#define VMK_MODPARAM_STRING(name, len, desc) \
   VMK_MODPARAM_STRING_NAMED(name, name, len, desc)

/*
 ***********************************************************************
 * VMK_VERSION_INFO --                                            */ /**
 *
 * \ingroup Module
 * \brief Free-form string parameter describing version, build,
 *        etc. information.
 *
 * \param[in] string    A string to be embedded as version information.
 *
 ***********************************************************************
 */
#define VMK_VERSION_INFO(string) \
   __VMK_VERSION_INFO(string)

/*
 ***********************************************************************
 * VMK_LICENSE_INFO --                                            */ /**
 *
 * \ingroup Module
 * \brief A predefined string describing the license this module is
 *        released under.
 *
 * This macro adds a license string to the module. The license string
 * determines symbol binding rules. For instance, a module released under
 * a non-GPL license can only bind to symbols exported by non-GPL modules,
 * but it can not bind to symbols provided by GPL modules. A module
 * released under a GPL license can bind to symbols exported by GPL and
 * non-GPL modules.
 *
 * \note This macro should only be used if you release your code under
 *       a license described by the predefined license strings such as
 *       VMK_MODULE_LICENSE_*, otherwise please use macro
 *       VMK_THIRD_PARTY_LICENSE_INFO.
 *
 * \param[in] string  A string to be embedded as the license type. Only
 *                    the predefined license strings such as
 *                    VMK_MODULE_LICENSE_* are acceptable. The use of
 *                    other license strings may cause the module to
 *                    fail to load.
 *
 ***********************************************************************
 */
#define VMK_LICENSE_INFO(string) \
   __VMK_LICENSE_INFO(string)

/*
 ***********************************************************************
 * VMK_THIRD_PARTY_LICENSE_INFO --                                */ /**
 *
 * \ingroup Module
 * \brief A string describing the license this module is released
 *        under.
 *
 * This macro adds a license string to the module. The license string
 * determines symbol binding rules. A module released under a third
 * party license can only bind to symbols exported by non-GPL modules.
 *
 * \note This macro should be used if you release your code under
 *       a license that is not covered by the predefined license strings
 *       such as VMK_MODULE_LICENSE_*, e.g., under your company's
 *       license.
 *
 * \note This macro accepts any provided string as a valid license
 *       string, in contrast to macro VMK_LICENSE_INFO which only
 *       accepts predefined license strings.
 *
 * \param[in] string  A string to be embedded as the license type. The
 *                    predefined license strings as VMK_MODULE_LICENSE_*
 *                    should not be used with this macro.
 *
 ***********************************************************************
 */
#define VMK_THIRD_PARTY_LICENSE_INFO(string) \
   __VMK_LICENSE_INFO(__VMK_MODULE_LICENSE_THIRD_PARTY ":" string)

/*
 ***********************************************************************
 * vmk_ModuleRegister --                                          */ /**
 *
 * \ingroup Module
 * \brief Register a module with the VMKernel
 *
 * \pre The module shall not call any VMKernel function before this
 *      function has been invoked and has returned.
 *
 * \note A module should make a successful call to this function only
 *       once inside its initalization function, else undefined
 *       behavior may occur.
 *
 * \note  This function may block.
 *
 * \param[out] id                The address of a variable to store
 *                               the module's module ID handle.
 * \param[in] vmkApiModRevision  The module version for compatability
 *                               checks.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ModuleRegister(
   vmk_ModuleID *id,
   vmk_uint32 vmkApiModRevision);

/*
 ***********************************************************************
 * vmk_ModuleUnregister --                                        */ /**
 *
 * \ingroup Module
 * \brief Unregister a module with the VMKernel
 *
 * \pre The module shall not have any VMKernel call in progress at
 *      the time this function is invoked, nor initiate any VMKernel
 *      call after it has been invoked.
 *
 * \note The module ID handle will be invalid after the success of
 *       this call and should not be used again.
 *
 * \note  This function may block.
 *
 * \param[in] id  The module ID handle to unregister.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ModuleUnregister(
   vmk_ModuleID id);

/*
 ***********************************************************************
 * vmk_ModuleSetHeapID --                                         */ /**
 *
 * \ingroup Module
 * \brief Set a module's default heap
 *
 * Any vmkapi call that does not take an explicit heap that also has
 * a side effect of allocating storage will use the heap passed to this
 * function.
 *
 * \note  This function will not block.
 *
 * \pre The default heap may only be assigned once.  Subsequent
 *      assignments will be ignored.
 *
 ***********************************************************************
 */
void vmk_ModuleSetHeapID(
   vmk_ModuleID module,
   vmk_HeapID heap);

/*
 ***********************************************************************
 * vmk_ModuleGetHeapID --                                         */ /**
 *
 * \ingroup Module
 * \brief Query a module's default heap
 *
 * \note  This function will not block.
 *
 * \return The calling module's current default heap.
 * \retval VMK_INVALID_HEAP_ID The module has no default heap.
 *
 ***********************************************************************
 */
vmk_HeapID vmk_ModuleGetHeapID(
   vmk_ModuleID module);

/*
 ***********************************************************************
 * vmk_ModuleGetID --                                             */ /**
 *
 * \ingroup Module
 * \brief Get the identifier of the VMKernel module
 *
 * \note  This function will not block.
 *
 * \param[in] moduleName   Name of the module to find.
 *
 * \return The module ID of the module with the specified name.
 * \retval VMK_INVALID_MODULE_ID    No module with the specified name
 *                                  was found.
 *
 ***********************************************************************
 */
vmk_ModuleID vmk_ModuleGetID(
   const char *moduleName);


/*
 ***********************************************************************
 * vmk_ModuleGetName --                                           */ /**
 *
 * \ingroup Module
 * \brief Get the name associated with a module.
 *
 * \note  This function will not block.
 *
 * \note This call will return an error when called to retrieve the
 *       name of a module that has not yet returned from the module
 *       init function.
 *
 * \param[in]  module      The module ID to query.
 * \param[out] moduleName  A character buffer large enough to hold the
 *                         module name including the terminating nul.
 * \param[in]  len         The length of the character buffer in bytes.
 *
 * \retval VMK_NOT_FOUND   The module ID was not found.
 * \retval VMK_BAD_PARAM   The buffer isn't large enough to hold
 *                         the module's string name.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ModuleGetName(
   vmk_ModuleID module,
   char *moduleName,
   vmk_ByteCountSmall len);

/*
 ***********************************************************************
 * vmk_ModuleGetDebugID --                                        */ /**
 *
 * \ingroup Module
 * \brief Convert a vmk_ModuleID to a 64-bit integer representation.
 *        This should not be used be used for anything other than a
 *        short-hand in debugging output.
 *
 * \note  This function will not block.
 *
 * \param[in] module    The module id.
 *
 ***********************************************************************
 */
vmk_uint64 vmk_ModuleGetDebugID(vmk_ModuleID module);

/*
 ***********************************************************************
 * vmk_ModuleIncUseCount --                                       */ /**
 *
 * \ingroup Module
 * \brief Increment a module's reference count
 *
 * \note  This function will not block.
 *
 * Any attempt to remove the module with \c vmkload_mod -u will fail
 * while the module's reference count is non nul.
 *
 * \param[in] module    Module to increment the reference count for.
 *
 * \retval VMK_OK                   The reference count was successfully
 *                                  incremented
 * \retval VMK_NOT_FOUND            The module doesn't exist
 * \retval VMK_MODULE_NOT_LOADED    The module is being unloaded
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ModuleIncUseCount(
   vmk_ModuleID module);

/*
 ***********************************************************************
 * vmk_ModuleDecUseCount --                                       */ /**
 *
 * \ingroup Module
 * \brief Decrement a module's reference count.
 *
 * \note  This function will not block.
 *
 * \param[in] module    Module to decrement the reference count for.
 *
 * \retval VMK_OK                   The reference count was successfully
 *                                  decremented.
 * \retval VMK_NOT_FOUND            The module doesn't exist.
 * \retval VMK_MODULE_NOT_LOADED    The module is being unloaded.
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ModuleDecUseCount(
   vmk_ModuleID module);

/*
 ***********************************************************************
 * vmk_ModulePushId --                                            */ /**
 *
 * \ingroup Module
 * \brief Push moduleID onto module tracking stack before an
 *        inter-module call.
 *
 * \note  This function will not block.
 *
 * \deprecated This call should no longer be called directly as it is
 *             likely to go away in a future release.
 *
 * \param[in] moduleID     Module ID from which the inter-module call
 *                         is to be made.
 * \param[in] function     Address of the inter-module function call
 * \param[in] modStack     Pointer to a vmk_ModInfoStack struct,
 *                         preferrably on the stack.
 *
 * \retval VMK_OK                   The moduleID was sucessfully pushed
 *                                  onto the module stack
 * \retval VMK_MODULE_NOT_LOADED    Module was not found
 *
 ***********************************************************************
 */
VMK_ReturnStatus vmk_ModulePushId(
    vmk_ModuleID moduleID,
    void *function,
    vmk_ModInfoStack *modStack);

/*
 ***********************************************************************
 * vmk_ModulePopId --                                             */ /**
 *
 * \ingroup Module
 * \brief Pop moduleID off of module tracking stack after an
 *        inter-module call.
 *
 * \note  This function will not block.
 *
 * \deprecated This call should no longer be called directly as it is
 *             likely to go away in a future release.
 *
 ***********************************************************************
 */
void vmk_ModulePopId(void);

/*
 ***********************************************************************
 * vmk_ModuleStackTop --                                          */ /**
 *
 * \ingroup Module
 * \brief Get the latest moduleID pushed onto the module tracking stack.
 *
 * \note  This function will not block.
 *
 * \retval The moduleID at the top of the module tracking stack.
 *
 ***********************************************************************
 */
vmk_ModuleID vmk_ModuleStackTop(void);


/*
 ***********************************************************************
 * VMKAPI_MODULE_CALL --                                          */ /**
 *
 * \ingroup Module
 * \brief Macro wrapper for inter-module calls that return a value.
 *
 * This wrapper should always be used when calling into another module
 * so that vmkernel can properly track resources associated with
 * a call.
 *
 * \param[in]     moduleID       moduleID of the calling module.
 * \param[out]    returnValue    Variable to hold the return value from
 *                               the called function.
 * \param[in]     function       Inter-module function call to be
 *                               invoked.
 * \param[in,out] args           Arguments to pass to the inter-module
 *                               function call.
 *
 ***********************************************************************
 */
#define VMKAPI_MODULE_CALL(moduleID, returnValue, function, args...)    	\
do {                                                                    	\
    vmk_ModInfoStack modStack;						\
    vmk_ModulePushId(moduleID, function, &modStack) ;                   \
    VMK_DEBUG_ONLY(vmk_Bool intsEnabledOnEntry = vmk_CPUHasIntsEnabled();)      \
    returnValue = (function)(args);                                     	\
    VMK_DEBUG_ONLY(                                                             \
        if (intsEnabledOnEntry != vmk_CPUHasIntsEnabled()) {                    \
            vmk_Panic("Function %p in module %d %sabled interrupts",            \
                 function, moduleID, intsEnabledOnEntry ? "dis" : "en");        \
        }                                                                       \
    )                                                                           \
    vmk_ModulePopId();                                                  	\
} while(0)

/*
 ***********************************************************************
 * VMKAPI_MODULE_CALL_VOID --                                     */ /**
 *
 * \ingroup Module
 * \brief Macro wrapper for inter-module calls that do not return
 *        a value.
 *
 * This wrapper should always be used when calling into another module
 * so that vmkernel can properly track resources associated with
 * a call.
 *
 * \param[in]     moduleID    moduleID of the calling module
 * \param[in]     function    Inter-module function call to be invoked
 * \param[in,out] args        Arguments to pass to the inter-module
 *                            function call
 *
 ***********************************************************************
 */
#define VMKAPI_MODULE_CALL_VOID(moduleID, function, args...)    		\
do {                                                            		\
    vmk_ModInfoStack modStack;					\
    vmk_ModulePushId(moduleID, function, &modStack);            \
    VMK_DEBUG_ONLY(vmk_Bool intsEnabledOnEntry = vmk_CPUHasIntsEnabled();)      \
    (function)(args);                                           		\
    VMK_DEBUG_ONLY(                                                             \
        if (intsEnabledOnEntry != vmk_CPUHasIntsEnabled()) {                    \
            vmk_Panic("Function %p in module %d %sabled interrupts",            \
                 function, moduleID, intsEnabledOnEntry ? "dis" : "en");        \
        }                                                                       \
    )                                                                           \
    vmk_ModulePopId();                                          		\
} while(0)

/*
 ***********************************************************************
 * VMK_MODULE_EXPORT_SYMBOL --                                    */ /**
 *
 * \ingroup Module
 * \brief Mark a symbol as exported
 *
 * Mark the given symbol as exported (and hence available for other
 * modules to find/call) within the name-space and version of the
 * current module (as specified by VMK_NAMESPACE_PROVIDES())
 *
 * \note Although exported symbols are encapsulated within the modules
 * provided name-space, exported symbols are required to have a
 * globally unique name.  This is because there are no restrictions on
 * what combinations of name-spaces any given module may request,
 * leading to potential unresolvable collisions if a module requested
 * two name-spaces that provided the same symbol.
 *
 * \param[in] symname    The symbol to export.
 *
 ***********************************************************************
 */
#define VMK_MODULE_EXPORT_SYMBOL(symname)     \
   __VMK_MODULE_EXPORT_SYMBOL(symname)

/*
 ***********************************************************************
 * VMK_MODULE_EXPORT_SYMBOL_ALIASED --                            */ /**
 *
 * \ingroup Module
 * \brief Re-export a symbol under an aliased name
 *
 * Re-export the given original symbol "symname" as "alias" within the
 * name-space and version of the current module (as specified by
 * VMK_NAMESPACE_PROVIDES()).  "symname" must be either:
 *
 * - an internal global (i.e. not static) symbol of the current
 *   module, or
 * - be present by virtue of being exported by another module, and the
 *   current module must contain the correct VMK_NAMESPACE_REQUIRED()
 *   invocation such that the original symbol can be found.
 *
 * "alias" must be globally unqiue.
 *
 * \note Calls via this alias incur no overheads, as referencing the
 * alias results in the function address of the original symbol.
 *
 * \param[in] symname    The symbol to export.
 * \param[in] alias      The publicly exported name
 *
 ***********************************************************************
 */
#define VMK_MODULE_EXPORT_SYMBOL_ALIASED(symname, alias)    \
   __VMK_MODULE_EXPORT_SYMBOL_ALIASED(symname, alias)

/*
 ***********************************************************************
 * VMK_MODULE_EXPORT_ALIAS --                                     */ /**
 *
 * \ingroup Module
 * \brief Re-export an already exported symbol
 *
 * Re-export the given original symbol "symname" within the name-space
 * and version of the current module (as specified by
 * VMK_NAMESPACE_PROVIDES()).  The original symbol must be present by
 * virtue of being exported by another module, and the current module
 * must contain the correct VMK_NAMESPACE_REQUIRED() invocation such
 * that the original symbol can be found.
 *
 * \note Calls via this alias incur no overheads, as referencing the
 * alias results in the function address of the original symbol.
 *
 * \param[in] symname    The name of the original symbol to re-export
 *
 ***********************************************************************
 */
#define VMK_MODULE_EXPORT_ALIAS(symname) \
   __VMK_MODULE_EXPORT_SYMBOL_ALIASED(symname, symname)

/*
 ***********************************************************************
 * VMK_NAMESPACE_REQUIRED --                                      */ /**
 *
 * \ingroup Module
 * \brief Mark this module as requiring a name-space at a given version
 *
 * Mark the module as requiring a name-space "namespace" at "version".
 * There is no limit on the number of name-spaces a module may require.
 *
 * \note The namespace and version parameters may not contain the
 *       restricted character VMK_NS_VER_SEPARATOR. Including this
 *       character may cause the module to fail to load.
 *
 * \param[in] namespace  The name-space
 * \param[in] version    The version of this name-space
 *
 ***********************************************************************
 */
#define VMK_NAMESPACE_REQUIRED(namespace, version) \
   __VMK_NAMESPACE_REQUIRED(namespace, version)

/*
 ***********************************************************************
 * VMK_NAMESPACE_PROIVDES --                                      */ /**
 *
 * \ingroup Module
 * \brief Mark this module as providing a name-space at a given version
 *
 * Mark the module as providing a name-space "namespace" at "version".
 * Each module may provide only one name-space.
 *
 * \note The namespace and version parameters may not contain the
 *       restricted character VMK_NS_VER_SEPARATOR. Including this
 *       character may cause the module to fail to load.
 *
 * \param[in] namespace  The name-space
 * \param[in] version    The version of this name-space
 *
 ***********************************************************************
 */
#define VMK_NAMESPACE_PROVIDES(namespace, version) \
   __VMK_NAMESPACE_PROVIDES(namespace, version)

#endif /* _VMKAPI_MODULE_H_ */
/** @} */
