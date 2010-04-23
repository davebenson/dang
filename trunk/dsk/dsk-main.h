
/* dispatch wrappers */
...

/* program termination (terminate when ref-count gets to 0);
 * many programs leave 0 refs the whole time.
 */
void dsk_main_add_object (void *object);
void dsk_main_add_ref    (void);
void dsk_main_remove_ref (void);

/* running until termination (ie until we get to 0 refs) */
int  dsk_main_run        (void);
