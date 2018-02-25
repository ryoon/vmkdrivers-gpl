
/* **********************************************************
 * Copyright 2008 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 * Module internal macros, defines, etc.
 *
 * Do not directly include or use the interfaces
 * provided in this header; only use those provided by
 * vmkapi_module.h.
 */
/** \cond nodoc */

#ifndef _VMKAPI_MODULE_INT_H_
#define _VMKAPI_MODULE_INT_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 * Internal macro-machinery for parameter handling
 */
#define __vmk_ModuleParamType_int          1
#define __vmk_ModuleParamType_int_array    2
#define __vmk_ModuleParamType_uint         3
#define __vmk_ModuleParamType_uint_array   4
#define __vmk_ModuleParamType_long         5
#define __vmk_ModuleParamType_long_array   6
#define __vmk_ModuleParamType_ulong        7
#define __vmk_ModuleParamType_ulong_array  8
#define __vmk_ModuleParamType_short        9
#define __vmk_ModuleParamType_short_array  10
#define __vmk_ModuleParamType_ushort       11
#define __vmk_ModuleParamType_ushort_array 12
#define __vmk_ModuleParamType_string       13
#define __vmk_ModuleParamType_charp        14
#define __vmk_ModuleParamType_bool         15
#define __vmk_ModuleParamType_byte         16

#define VMK_PARAM(type) VMK_PARAM_##type = __vmk_ModuleParamType_##type

typedef enum {
   VMK_PARAM(int),
   VMK_PARAM(int_array),
   VMK_PARAM(uint),
   VMK_PARAM(uint_array),
   VMK_PARAM(long),
   VMK_PARAM(long_array),
   VMK_PARAM(ulong),
   VMK_PARAM(ulong_array),
   VMK_PARAM(short),
   VMK_PARAM(short_array),
   VMK_PARAM(ushort),
   VMK_PARAM(ushort_array),
   VMK_PARAM(string),
   VMK_PARAM(charp),
   VMK_PARAM(bool),
   VMK_PARAM(byte)
} vmk_ModuleParamType;

struct vmk_ModuleParam {
   const char *name;
   vmk_ModuleParamType type;
   union {
      void *arg;
      struct {
	 char *arg;
	 int maxlen;
      } string;
      struct {
	 void *arg;
	 int maxlen;
	 int *nump;
      } array;
   } param;
};

#define VMK_PARAM_SEC ".vmkmodparam"
#define VMK_MODINFO_SEC ".vmkmodinfo"

/*
 * License tag for third party modules.
 */
#define __VMK_MODULE_LICENSE_THIRD_PARTY "ThirdParty"

/*
 * Add a string to modinfo without a corresponding symbol (i.e.
 * informational only)
 */
#define __VMK_MODINFO_STAMP(string)                             \
   asm(".pushsection " VMK_MODINFO_SEC ",\"a\", @progbits\n"    \
       "\t.string \"" string  "\" \n"                           \
       "\t.popsection\n")

/*
 * Named parameter
 */
#define __VMK_MODPARAM_NAME(name, type)         \
   __VMK_MODINFO_STAMP("param_" #name ":" type)

/*
 * Description of named paramter.  Note: can not use stamp method as
 * 'desc' may have embedded newlines which the asm calls expands...
 */
#define __VMK_MODPARAM_DESC(name, desc)         \
   const char __module_desc_##name[]            \
   VMK_ATTRIBUTE_SECTION(VMK_MODINFO_SEC)       \
      = "param_desc_" #name "=" desc

/* Required attributes for variables in VMK_PARAM_SEC */
#define __VMK_MODPARAM_ATTRS              \
   VMK_ATTRIBUTE_SECTION(VMK_PARAM_SEC)   \
   VMK_ATTRIBUTE_USED                     \
   VMK_ATTRIBUTE_ALIGN(sizeof(void*))

/* Basic type */
#define __VMK_MODPARAM_NAMED(__name, __var, __type)	\
   static char __vmk_param_str_##__name[] = #__name;	\
   static struct vmk_ModuleParam const __param_##__name	\
     __VMK_MODPARAM_ATTRS				\
      = {						\
         .name = __vmk_param_str_##__name,		\
         .type = __vmk_ModuleParamType_##__type,	\
         .param.arg = &__var,				\
        };						\
   __VMK_MODPARAM_NAME(__name, #__type)

/* Array */
#define __VMK_MODPARAM_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define __VMK_MODPARAM_ARRAY_NAMED(__name, __array, __type, __nump)	\
   static char __vmk_param_str_##__name[] = #__name;			\
   static struct vmk_ModuleParam const __param_##__name			\
     __VMK_MODPARAM_ATTRS						\
      = {								\
          .name = __vmk_param_str_##__name,				\
          .type = __vmk_ModuleParamType_##__type##_array,		\
          .param.array.arg = __array,					\
          .param.array.maxlen = __VMK_MODPARAM_ARRAY_SIZE(__array),	\
          .param.array.nump = __nump,					\
        };								\
   __VMK_MODPARAM_NAME(__name, "array of " #__type)

/* String */
#define __VMK_MODPARAM_STRING_NAMED(__name, __string, __max)	\
   static char __vmk_param_str_##__name[] = #__name;		\
   static struct vmk_ModuleParam const __param_##__name		\
     __VMK_MODPARAM_ATTRS					\
      = {							\
         .name = __vmk_param_str_##__name,			\
         .type = __vmk_ModuleParamType_string,			\
         .param.string.arg = __string,				\
         .param.string.maxlen = __max,				\
        };							\
   __VMK_MODPARAM_NAME(__name, "string")


/*
 * Version and Build Type Information
 */

#define VMK_VERSION_INFO_SYM              __vmk_versionInfo_str
#define VMK_VERSION_INFO_TAG              "version="
#define VMK_VERSION_INFO_TAG_LEN          (sizeof(VMK_VERSION_INFO_TAG)-1)

#define VMK_BUILD_TYPE_INFO_SYM           __vmk_buildTypeInfo_str
#define VMK_BUILD_TYPE_INFO_TAG           "buildType="
#define VMK_BUILD_TYPE_INFO_TAG_LEN       (sizeof(VMK_BUILD_TYPE_INFO_TAG)-1)

#ifndef VMK_BUILD_TYPE
#     ifdef VMX86_DEBUG
#         define VMK_BUILD_TYPE   "debug"
#     else
#         define VMK_BUILD_TYPE   "release"
#     endif
#endif

#define __VMK_VERSION_INFO(__string)                                                \
   const static char VMK_BUILD_TYPE_INFO_SYM[]                                      \
   VMK_ATTRIBUTE_USED                                                               \
   VMK_ATTRIBUTE_SECTION(VMK_MODINFO_SEC) = VMK_BUILD_TYPE_INFO_TAG VMK_BUILD_TYPE; \
   const static char VMK_VERSION_INFO_SYM[]                                         \
   VMK_ATTRIBUTE_USED                                                               \
   VMK_ATTRIBUTE_SECTION(VMK_MODINFO_SEC) = VMK_VERSION_INFO_TAG __string

/*
 * License Information
 */

#define VMK_LICENSE_INFO_SYM	  __vmk_licenseInfo_str
#define VMK_LICENSE_INFO_TAG      "license="
#define VMK_LICENSE_INFO_TAG_LEN  (sizeof(VMK_LICENSE_INFO_TAG)-1)

#define __VMK_LICENSE_INFO(__string)                                       \
   const static char VMK_LICENSE_INFO_SYM[]                                \
   VMK_ATTRIBUTE_USED                                                      \
   VMK_ATTRIBUTE_SECTION(VMK_MODINFO_SEC) = VMK_LICENSE_INFO_TAG __string

/*
 * Symbol Exports
 */

#define VMK_EXPORT_SYMBOL_SEC ".vmksymbolexports"

struct vmk_ExportSymbol {
   char *name;
   enum {
      VMK_EXPORT_SYMBOL_DEFAULT,
      /* Direct export means don't ever create a trampoline for this symbol. */
      VMK_EXPORT_SYMBOL_DIRECT,
   } exportType;
};

#define __VMK_EXPORT_SYMBOL_ATTRS               \
   VMK_ATTRIBUTE_SECTION(VMK_EXPORT_SYMBOL_SEC) \
   VMK_ATTRIBUTE_USED                           \
   VMK_ATTRIBUTE_ALIGN(sizeof(void*))

#define __VMK_MODULE_EXPORT_SYMBOL(__symname)                           \
   static char __vmk_symbol_str_##__symname[] = #__symname;             \
   static struct vmk_ExportSymbol const __vmk_symbol_##__symname        \
   __VMK_EXPORT_SYMBOL_ATTRS                                            \
   = {                                                                  \
      .name = __vmk_symbol_str_##__symname,                             \
      .exportType = VMK_EXPORT_SYMBOL_DEFAULT,                          \
   };

#define __VMK_MODULE_EXPORT_SYMBOL_DIRECT(__symname)                    \
   static char __vmk_symbol_str_##__symname[] = #__symname;             \
   static struct vmk_ExportSymbol const __vmk_symbol_##__symname        \
   __VMK_EXPORT_SYMBOL_ATTRS                                            \
   = {                                                                  \
      .name = __vmk_symbol_str_##__symname,                             \
      .exportType = VMK_EXPORT_SYMBOL_DIRECT,                           \
   };

/*
 * Aliases
 */

#define VMK_EXPORT_ALIAS_SEC ".vmksymbolaliases"

/*
 * Note that VMK_MODULE_EXPORT_ALIAS() sets the name and alias the
 * same; we assume this condition is a direct external alias and that
 * 'name' is exported by another module.
 */

struct vmk_ExportSymbolAlias {
   char *name;
   char *alias;
};

#define __VMK_EXPORT_ALIAS_ATTRS                \
   VMK_ATTRIBUTE_SECTION(VMK_EXPORT_ALIAS_SEC)  \
   VMK_ATTRIBUTE_USED                           \
   VMK_ATTRIBUTE_ALIGN(sizeof(void*))

#define __VMK_MODULE_EXPORT_SYMBOL_ALIASED(__symname, __alias)          \
   static char __vmk_symbol_str_##__symname##__alias[] = #__symname;    \
   static char __vmk_symbol_str_##__alias[] = #__alias;                 \
   static struct vmk_ExportSymbolAlias const                            \
   __vmk_symbol_##__symname##__alias                                    \
   __VMK_EXPORT_ALIAS_ATTRS                                             \
   = {                                                                  \
      .name = __vmk_symbol_str_##__symname##__alias,                    \
      .alias = __vmk_symbol_str_##__alias,                              \
   };


/*
 * Name-spaces
 */

#define VMK_NAMESPACE_SEC ".vmkrequiredns"

#define __VMK_NAMESPACE_REQUIRED(__ns, __version)               \
   asm(".pushsection " VMK_NAMESPACE_SEC ",\"aS\", @progbits\n" \
       "\t.string \"" __ns VMK_NS_VER_SEPARATOR_STRING          \
                      __version  "\" \n"                        \
       "\t.popsection\n");                                      \
   __VMK_MODINFO_STAMP("nsRequired=" __ns                       \
                       VMK_NS_VER_SEPARATOR_STRING __version)


#define VMK_NAMESPACE_PROVIDES_SYM	__vmk_nsProvides_str
#define VMK_NAMESPACE_PROVIDES_TAG	"nsProvides="
#define VMK_NAMESPACE_PROVIDES_TAG_LEN	(sizeof(VMK_NAMESPACE_PROVIDES_TAG)-1)

#define __VMK_NAMESPACE_PROVIDES(__ns, __version)  \
   const static char VMK_NAMESPACE_PROVIDES_SYM[]  \
      VMK_ATTRIBUTE_USED                           \
      VMK_ATTRIBUTE_SECTION(VMK_MODINFO_SEC) =     \
         VMK_NAMESPACE_PROVIDES_TAG __ns           \
         VMK_NS_VER_SEPARATOR_STRING __version;


#endif /* _VMKAPI_MODULE_INT_H_ */
/** \endcond nodoc */
