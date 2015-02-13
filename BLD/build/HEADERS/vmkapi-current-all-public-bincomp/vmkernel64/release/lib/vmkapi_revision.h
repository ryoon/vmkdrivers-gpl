/* **********************************************************
 * Copyright 2004 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ***********************************************************************
 * Define and document the Revision group
 *                                                                */ /**
 * \addtogroup Lib
 * @{
 * \defgroup Revision Revision Numbering
 *
 * If an interface is exported it should be accompanied by a declaration
 * of it's revision. This is done for the vmkapi itself and some of its
 * constituent interfaces such as the SCSI interfaces.\n
 * \n
 * To declare a revision, pick a representative prefix. The prefix
 * is usually related to the interface being exported. For the sake
 * of example, let's usee the prefix "FOO".\n
 * \n
 * The prefix should be used to declare the major, minor, patch-level
 * and development level for the interface. This is done by declaring
 * four macros for each part of the interface's revision number. Each
 * number should be between 0-255.\n
 * \n
 * For example, here is the revision declaration for the interface
 * FOO at revision 1.2.0-25:\n
 *\n
 * \code
 * #define FOO_REVISION_MAJOR       1
 * #define FOO_REVISION_MINOR       2
 * #define FOO_REVISION_UPDATE      0
 * #define FOO_REVISION_PATCH_LEVEL 25
 * \n
 * Often, it is useful to compare revision numbers or store them in a
 * compact form. This is often represented by an additional macro
 * declaration as follows:
 * \n
 * \code
 * #define FOO_REVISION    VMK_REVISION_NUMBER(FOO)
 * \endcode
 *
 * @{
 ***********************************************************************
 */

#ifndef _VMKAPI_REVISION_H_
#define _VMKAPI_REVISION_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/*
 * Internal machinery for revision utility macros.
 */
#define VMK_REVISION_STRINGIFY(x) #x
#define VMK_REVISION_EXPANDSTR(x) VMK_REVISION_STRINGIFY(x)

/*
 ***********************************************************************
 * VMK_REVISION_STRING --                                         */ /**
 *
 * \brief Convert a interface's revision to a string
 *
 * \param id The prefix for the interface to be converted
 *
 * \return A printable string that represets the interface's revision
 * 
 * \par Example:
 * \code
 * #define FOO_REVISION_MAJOR       1
 * #define FOO_REVISION_MINOR       2
 * #define FOO_REVISION_UPDATE      0
 * #define FOO_REVISION_PATCH_LEVEL 25
 * 
 * char *api_string = VMK_REVISION_STRING(FOO);
 * \endcode
 *
 ***********************************************************************
 */
#define VMK_REVISION_STRING(id) \
                           VMK_REVISION_EXPANDSTR(id##_REVISION_MAJOR) "." \
                           VMK_REVISION_EXPANDSTR(id##_REVISION_MINOR) "." \
                           VMK_REVISION_EXPANDSTR(id##_REVISION_UPDATE) "-" \
                           VMK_REVISION_EXPANDSTR(id##_REVISION_PATCH_LEVEL)

/*
 ***********************************************************************
 * VMK_REVISION_NUMBER --                                         */ /**
 *
 * \brief Convert a interface's revision to a vmk_revnum
 *
 * \param id The prefix for the interface to be converted
 *
 * \return The interface's version number encoded in a vmk_revnum
 * 
 * \par Example:
 * \code
 * #define FOO_REVISION_MAJOR       1
 * #define FOO_REVISION_MINOR       2
 * #define FOO_REVISION_UPDATE      0
 * #define FOO_REVISION_PATCH_LEVEL 25
 * 
 * vmk_revnum fooRev = VMK_REVISION_NUMBER(FOO);
 * \endcode
 *
 ***********************************************************************
 */
#define VMK_REVISION_NUMBER(id) ((((id##_REVISION_MAJOR) & 0xFF) << 24) | \
                                 (((id##_REVISION_MINOR) & 0xFF) << 16) | \
                                 (((id##_REVISION_UPDATE) & 0xFF) << 8) | \
                                  ((id##_REVISION_PATCH_LEVEL) & 0xFF))

/*
 ***********************************************************************
 * VMK_REVISION_FROM_NUMBERS --                                   */ /**
 *
 * \brief Create a revision number from specified version numbers.
 *
 * \param major         Major number.
 * \param minor         Minor number.
 * \param update        Update number.
 * \param patch_level   Patch_level number.
 *
 * \return A revision number that corresponds to the given parameters.
 * 
 * \par Example:
 * \code
 * #if VMKAPI_REVISION == VMK_REVISION_FROM_NUMBERS(1,0,0,0)
 * \endcode
 *
 ***********************************************************************
 */
#define VMK_REVISION_FROM_NUMBERS(major, minor, update, patch_level) \
   ((((major) & 0xFF) << 24) | \
    (((minor) & 0xFF) << 16) | \
    (((update) & 0xFF) << 8) | \
     ((patch_level) & 0xFF))

/*
 * \brief Type to use when storing revision numbers
 */
typedef vmk_uint32 vmk_revnum;

/*
 ***********************************************************************
 * vmk_RevisionsAreEqual --                                       */ /**
 *
 * \brief Determine if two revision numbers are equal
 *
 * \param rev1 The first revision number to compare
 * \param rev2 The second revision number to compare
 *
 * \retval VMK_TRUE if the revision numbers are equal
 * \retval VMK_FALSE if the revision numbers are not equal
 *
 ***********************************************************************
 */
static inline
vmk_Bool
vmk_RevisionsAreEqual(
   vmk_revnum rev1,
   vmk_revnum rev2)
{
   return (rev1 == rev2) ? VMK_TRUE : VMK_FALSE ;
}

/*
 ***********************************************************************
 * VMK_REVISION_MAJOR --                                         */ /**
 *
 * \brief Extract the major revision number from a vmk_revnum
 *
 * \param rev A variable of type vmk_revnum to extract the major
 *            revision number from
 *
 * \return The interface's major revision number
 *
 ***********************************************************************
 */
#define VMK_REVISION_MAJOR(rev) (((rev) >> 24) & 0xFF)

/*
 ***********************************************************************
 * VMK_REVISION_MINOR --                                         */ /**
 *
 * \brief Extract the minor revision number from a vmk_revnum
 *
 * \param rev A variable of type vmk_revnum to extract the minor
 *            revision number from
 *
 * \return The interface's minor revision number
 *
 ***********************************************************************
 */
#define VMK_REVISION_MINOR(rev) (((rev) >> 16) & 0xFF)

/*
 ***********************************************************************
 * VMK_REVISION_UPDATE --                                         */ /**
 *
 * \brief Extract the update revision number from a vmk_revnum
 *
 * \param rev A variable of type vmk_revnum to extract the update
 *            revision number from
 *
 * \return The interface's update revision number
 *
 ***********************************************************************
 */
#define VMK_REVISION_UPDATE(rev) (((rev) >> 8) & 0xFF)

/*
 ***********************************************************************
 * VMK_REVISION_PATCH_LEVEL --                                    */ /**
 *
 * \brief Extract the patch-level revision number from a vmk_revnum
 *
 * \param rev A variable of type vmk_revnum to extract the patch-level
 *            revision number from
 *
 * \return The interface's patch-level revision number
 *
 ***********************************************************************
 */
#define VMK_REVISION_PATCH_LEVEL(rev) ((rev) & 0xFF)

#endif /* _VMKAPI_REVISION_H_ */
/* @} */
/* @} */
