#if !defined(CUSTOM_FONT_SIZE_H)


internal void
SetFonSize()
{
    Face_Description Face = {};
    Face.
        int32_t FontCount = get_available_font_count(App);
    get_available_font(App, FontCount, );
    
    
    FACE_ID ID = get_face_id(App, 0);
    if(set_global_face(App,ID,true))
    {
        
    }
}















#define CUSTOM_FONT_SIZE_H
#endif
