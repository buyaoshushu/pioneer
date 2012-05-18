#ifdef HAVE_OLD_GTK
#define gtk_combo_box_text_new(A) gtk_combo_box_new_text(A)
#define gtk_combo_box_text_append_text(A, B) gtk_combo_box_append_text(A, B)
#define GTK_COMBO_BOX_TEXT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COMBO_BOX, GtkComboBox))
#define gdk_pixmap_get_size(A, B, C) gdk_drawable_get_size(A, B, C)

#ifdef GSEAL_ENABLE
#error "Don't use --enable-deprecation-checks when using old versions of Gtk"
#endif
#define gdk_visual_get_depth(A) A->depth
#define gtk_table_get_size(A, B, C) row = A->nrows
#endif