#if defined(CONFIG_USER_ONLY)
#define TARGET_PAGE_BITS 12
#else
/* The ARM MMU allows 1k pages.  */
/* ??? Linux doesn't actually use these, and they're deprecated in recent
   architecture revisions.  Maybe a configure option to disable them.  */
#define TARGET_PAGE_BITS 10
#endif
