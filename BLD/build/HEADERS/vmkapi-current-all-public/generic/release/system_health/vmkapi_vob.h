/* **********************************************************
 * Copyright 2007 - 2009 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * @VMKAPIMOD_LICENSE@
 */

/*
 ******************************************************************************
 * Adding Vmkernel Observations                                          */ /**
 *
 * \addtogroup SystemHealth
 * @{
 * \defgroup VmkVob Adding Vmkernel Observations (VOBs)
 *
 * The vob interface provides a mean of notifying the kernel about
 * meaningful observations (called as Vmkernel Observations or VOBs).
 * These observations will be available as events in the customer visible
 * user interface through vCenter server client. VOBs should only be used
 * as an indication or infromation regarding a specific known problem.
 * It is important to note that they should not be used as Log or Warning.
 * Users of these APIs will have to provide a fully qualified URL/link to
 * a knowledge base article explaining the steps to remedy the problem
 * indicated/informed by the VOB.
 *
 * @{
 ******************************************************************************
 */


#ifndef _VMKAPI_VOB_H_
#define _VMKAPI_VOB_H_

/** \cond never */
#ifndef VMK_HEADER_INCLUDED_FROM_VMKAPI_H
#error This vmkapi file should never be included directly but only via vmkapi.h
#endif
/** \endcond never */

/** \brief Max length of provider/module name which is adding the observation. */
#define VMK_MAX_VOB_SOURCE_NAME (32)

/** \brief Max length of Knowledge base URL link. */
#define VMK_MAX_KB_LINK_LEN (512)

/** \brief Max length format and arguments. */
#define VMK_MAX_VOB_FMTARGS_LEN (512)

/** \brief Observation urgency level. */
typedef enum {
   /**
    * Invalid urgency value.
    */
   VMK_VOB_URGENCY_INVALID = 0,

   /**
    * Observation is treated as information.
    * The event generated will be shown as information.
    */
   VMK_VOB_URGENCY_INFO = 1,

   /**
    * Observation is treated as warning. The event generated will be
    * shown as warning.
    */
   VMK_VOB_URGENCY_WARNING = 2,

   /**
    * Observation is treated as error. The event generated will be
    * shown as error.
    */
   VMK_VOB_URGENCY_ERROR = 3
} vmk_VobUrgency ;

/**
 * \brief Metadata which drive how the observation will be added.
 */
typedef struct vmk_VobMetadata {
   /** Urgency level of the observation. */
   vmk_VobUrgency urgency;

   /**
    * Module/Provider name to be associated with the event which will be
    * generated for the event. Use a descriptive module name for this.
    * Should be a NULL terminated string upto maximum length of
    * VMK_MAX_VOB_SOURCE_NAME.
    */
   const char source[VMK_MAX_VOB_SOURCE_NAME];

   /**
    * Link to a knowledge base article which describes the remedy steps
    * or explanation about the observation. Providing a Knowledge Base
    * URL link is mandatory. It should be a fully resolved URL.
    * This should be a NULL terminated string upto maximum of
    * VMK_MAX_KB_LINK_LEN.
    */
   const char kbLinkUrl[VMK_MAX_KB_LINK_LEN];

}
vmk_VobMetadata;


/*
 ***********************************************************************
 * vmk_VobNotify --                                               */ /**
 *
 * \ingroup VmkVob
 * \brief Notify kernel about an observation.
 *
 * Should be used to notify kernel about a critical problem, or some
 * critical observation which may help in identifying a root cause
 * of a problem. This call should not be used for reporting general
 * error conditions without specific, known solutions. Instead, logging
 * should be used in those conditions.
 *
 * \printformatstringdoc
 *
 * \param[in] metadata           An instance of vmk_VobMetadata.
 * \param[in] fmt                Format string.
 *
 * \retval VMK_OK                If the observation was added
 *                               successfully.
 * \retval VMK_BAD_PARM          If the specified parmeters were
 *                               incorrect.
 * \retval VMK_NAME_TOO_LONG     If the source name is longer than
 *                               limit of VMK_MAX_VOB_SOURCE_NAME
 * \retval VMK_LIMIT_EXCEEDED    If the URL length is longer than the
 *                               limit of VMK_MAX_KB_LINK_LEN
 * \retval VMK_MESSAGE_TOO_LONG  If the fmt string and variable
 *                               arguments are more than
 *                               VMK_MAX_VOB_FMTARGS_LEN.
 * \retval VMK_FAILURE           If a general failure occurred while
 *                               adding VOB.
 ***********************************************************************
 */

VMK_ReturnStatus  vmk_VobNotify(
   vmk_VobMetadata *metadata,
   const char *fmt,
   ...)
VMK_ATTRIBUTE_PRINTF(2, 3);

#endif /* _VMKAPI_VOB_H_ */
/** @} */
/** @} */
