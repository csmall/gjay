#ifndef __PLAYLIST__H__
#define __PLAYLIST__H__

GList *     generate_playlist ( guint len );
void        save_playlist     ( GList * list, 
                                gchar * fname );

#endif /* __PLAYLIST__H__ */
