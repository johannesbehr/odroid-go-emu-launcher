#include "goemu_ui.h"
#include "esp_system.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/rtc_io.h"
#include "../components/odroid/odroid_ui.h"
#include "../components/odroid/odroid_system.h"
#include "../components/odroid/odroid_display.h"
#include "../components/odroid/odroid_input.h"
#include "../components/odroid/odroid_audio.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include "gifdec.h"

#undef color_default
#undef color_selected
#undef color_bg_default

#define color_default C_GRAY
#define color_selected C_WHITE
#define color_bg_default (0.75*i) 

#define ROM_PATH "/sd/roms"




void goemu_ui_choose_file_init(goemu_emu_data_entry *emu) {
    if (emu->initialized)
    {
        return;
    }
    emu->initialized = true;
    if (emu->files.count == 0)
    {
        char buf[128];
        sprintf(buf, "%s/%s", ROM_PATH, emu->path);
        goemu_util_readdir(&emu->files, buf, emu->ext);
        goemu_util_strings_sort(&emu->files);
        if (emu->files.count > 0)
        {
            emu->checksums = (uint32_t*)heap_caps_malloc(emu->files.count*4, MALLOC_CAP_SPIRAM);
            memset(emu->checksums, 0, emu->files.count*4);
        }
    }
    goemu_config_load(emu);

    if (emu->selected >= emu->files.count)
    {
        emu->selected = 0;
    }
    
    int count = emu->files.count;
    uint32_t *entries_refs = emu->files.refs;

    odroid_ui_clean_draw_buffer();
    odroid_gamepad_state lastJoysticState;
    odroid_input_gamepad_read(&lastJoysticState);
    
    if (emu->selected == 0)
    {
        char *selected_file = odroid_settings_RomFilePath_get();
        if (selected_file) {
            if (strlen(selected_file) <strlen(emu->ext)+1 ||
                strcasecmp(emu->ext, &selected_file[strlen(selected_file)-strlen(emu->ext)])!=0 ) {
               printf("odroid_ui_choose_file: Ignoring last file: '%s'\n", selected_file);
               free(selected_file);
               selected_file = NULL;
            } else {
                // Found a match
                   for (int i = 0;i < count; i++) {
                      char *file = (char*)entries_refs[i];
                      if (strlen(selected_file) < strlen(file)) continue;
                      char *file2 = &selected_file[strlen(selected_file)-strlen(file)];
                      if (strcmp(file, file2) == 0) {
                          emu->selected = i;
                          printf("Last selected: %d: '%s'\n", emu->selected, file);
                          break;
                      }
                   }
                   free(selected_file);
                   selected_file = NULL;
            }
        }
    }
}

char *goemu_ui_choose_file_getfile(goemu_emu_data_entry *emu)
{
    char *file = (char*)emu->files.refs[emu->selected];
    char *rc = (char*)malloc(strlen(emu->path) + 1+ strlen(file)+1+strlen(ROM_PATH)+1);
    sprintf(rc, "%s/%s/%s", ROM_PATH, emu->path, file);
    return rc;
}

char *goemu_ui_choose_file_getfile_plus_extension(goemu_emu_data_entry *emu, char* extension)
{
	char *file = (char*)emu->files.refs[emu->selected];
	char *dot = strrchr(file,'.'); 
 	int flen = dot-file;
	char *rc = (char*)malloc(strlen(emu->path) + 1+ flen +1+strlen(ROM_PATH)+1 + strlen(extension) + 1);
    sprintf(rc, "%s/%s/%s", ROM_PATH, emu->path, file);
	sprintf(rc + strlen(emu->path) + 1 + flen + 1 + strlen(ROM_PATH)+1, "%s", extension);
    return rc;
}

void goemu_ui_choose_file_input(goemu_emu_data_entry *emu, odroid_gamepad_state *joystick, int *last_key_)
{
    int last_key = *last_key_;
    int selected = emu->selected;
    if (joystick->values[ODROID_INPUT_B]) {
        last_key = ODROID_INPUT_B;
            //entry_rc = ODROID_UI_FUNC_TOGGLE_RC_MENU_CLOSE;
    } else if (joystick->values[ODROID_INPUT_VOLUME]) {
        last_key = ODROID_INPUT_VOLUME;
           // entry_rc = ODROID_UI_FUNC_TOGGLE_RC_MENU_CLOSE;
    } else if (joystick->values[ODROID_INPUT_UP]) {
            last_key = ODROID_INPUT_UP;
            selected--;
            if (selected<0) selected = emu->files.count - 1;
    } else if (joystick->values[ODROID_INPUT_DOWN]) {
            last_key = ODROID_INPUT_DOWN;
            selected++;
            if (selected>=emu->files.count) selected = 0;
    } else if (joystick->values[ODROID_INPUT_LEFT]) {
        last_key = ODROID_INPUT_LEFT;
        char st = ((char*)emu->files.refs[selected])[0];
        int max = 20;
        while (selected>0 && max-->0)
        {
           selected--;
           if (st != ((char*)emu->files.refs[selected])[0]) break;
        }
        //selected-=10;
        //if (selected<0) selected = 0;
    } else if (joystick->values[ODROID_INPUT_RIGHT]) {
        last_key = ODROID_INPUT_RIGHT;
        char st = ((char*)emu->files.refs[selected])[0];
        int max = 20;
        while (selected<emu->files.count-1 && max-->0)
        {
           selected++;
           if (st != ((char*)emu->files.refs[selected])[0]) break;
        }
        //selected+=10;
        //if (selected>=emu->files.count) selected = emu->files.count - 1;
    }
    emu->selected = selected;
    *last_key_ = last_key;
}

/// Make the game name nicer by cutting at brackets or (last) dot.
int cut_game_name(char *game){

	char *dot = strrchr(game,'.'); 
	char *brack1 = strchr(game,'['); 
	char *brack2 = strchr(game,'('); 
	
	int len = strlen(game);
	if(dot!=NULL && dot-game<len ){
		len = dot-game;
	}
	if(brack1!=NULL && brack1-game<len ){
		len = brack1-game;
		if(game[len-1]==' '){
			len--;
		}
	}
	if(brack2!=NULL && brack2-game<len ){
		len = brack2-game;
		if(game[len-1]==' '){
			len--;
		}
	}
	return len;
}

void goemu_ui_choose_file_draw(goemu_emu_data_entry *emu)
{
    int count = emu->files.count;
    uint32_t *entries_refs = emu->files.refs;
    int x = 0;
    int lines = 30 - 7;
    int y_offset = 7*8;
	int y,entry;
    char *text;
	
	char *dot = NULL;
	uint8_t options_hide_extensions = true;
	uint8_t options_paging = true;
	        
			
	if(options_paging){
		
		y_offset+=8;
		int rows_per_page = (240 - y_offset) / (odroid_ui_framebuffer_height);
		int page = emu->selected / rows_per_page;
		
		
		for (int i = 0;i < rows_per_page + 1; i++) {
			y = y_offset + i * (odroid_ui_framebuffer_height);
			entry = i + (page * rows_per_page);//selected + i - 15;
			int length = 1;
			if (entry>=0 && entry < count && i < rows_per_page)
			{
				text = (char*)entries_refs[entry];
				if(options_hide_extensions){
					length = cut_game_name(text);
				}else{
					length = strlen(text);
				}
			}else{
				text = " ";
			}
			
			odroid_ui_draw_chars2(x, y, text, length, entry==emu->selected?C_YELLOW:C_WHITE, color_bg_default);
		}
	}else{
		for (int i = 0;i < lines; i++) {
			y = y_offset + i * 8;
			entry = emu->selected + i - lines/2;
						
			if (entry>=0 && entry < count)
			{
				text = (char*)entries_refs[entry];
				if(options_hide_extensions){
					// Hide extension
					dot = strrchr(text,'.'); 
					*dot = '\0';
				}
			} else
			{
				text = " ";
			}
		
			odroid_ui_draw_chars(x, y, 40, text, entry==emu->selected?color_selected:color_default, color_bg_default);
			
			// Restore extension
			if(dot!=NULL){
				*dot = '.';
			}
		}
	}
	
	
	
	
        
}

void goemu_ui_choose_file_load(goemu_emu_data_entry *emu)
{
    if (emu->image_logo == NULL)
    {
        char file[128];
		sprintf(file, "/sd/odroid/metadata/%s/logo.gif", emu->path_metadata);
		
        /*sprintf(file, "/sd/odroid/metadata/%s/logo.raw", emu->path_metadata);

        FILE *f = fopen(file,"rb");
        if (f)
        {
            emu->image_logo = (uint16_t*)heap_caps_malloc(GOEMU_IMAGE_LOGO_WIDTH*GOEMU_IMAGE_LOGO_HEIGHT*2, MALLOC_CAP_SPIRAM);
            fread(emu->image_logo, sizeof(uint16_t), GOEMU_IMAGE_LOGO_WIDTH*GOEMU_IMAGE_LOGO_HEIGHT, f);
            fclose(f);
        }
        else
        {
            printf("Image Logo '%s' not found\n", file);
        }*/
		
		gd_GIF *gif;
		uint8_t *frame;

		gif = gd_open_gif(file);
		if (gif) {
			printf("Gif is: %dx%d\r\n", gif->width, gif->height);
			if (gif->width==GOEMU_IMAGE_LOGO_WIDTH && gif->height==GOEMU_IMAGE_LOGO_HEIGHT){
				frame = heap_caps_malloc(gif->width*gif->height*2, MALLOC_CAP_SPIRAM);
				emu->image_logo = (uint16_t*)frame;
				int ret = gd_get_frame(gif);
				if (ret != -1){
					gd_render_frame(gif, frame);
				}
			}
			gd_close_gif(gif);
		}else{
			 printf("Image Logo '%s' not found\n", file);
		}
    }
    
    if (emu->image_header == NULL)
    {
        char file[128];
        sprintf(file, "/sd/odroid/metadata/%s/header.gif", emu->path_metadata);

        /*sprintf(file, "/sd/odroid/metadata/%s/header.raw", emu->path_metadata);
        FILE *f = fopen(file,"rb");
        if (f)
        {
            emu->image_header = (uint16_t*)heap_caps_malloc(GOEMU_IMAGE_HEADER_WIDTH*GOEMU_IMAGE_HEADER_HEIGHT*2, MALLOC_CAP_SPIRAM);
            fread(emu->image_header, sizeof(uint16_t), GOEMU_IMAGE_HEADER_WIDTH*GOEMU_IMAGE_HEADER_HEIGHT, f);
            fclose(f);
        }
        else
        {
            printf("Image Header '%s' not found\n", file);
        }*/
		
		gd_GIF *gif;
		uint8_t *frame;

		gif = gd_open_gif(file);
		if (gif) {
			printf("Gif is: %dx%d\r\n", gif->width, gif->height);
			if (gif->width==GOEMU_IMAGE_HEADER_WIDTH && gif->height==GOEMU_IMAGE_HEADER_HEIGHT){
				frame = heap_caps_malloc(gif->width*gif->height*2, MALLOC_CAP_SPIRAM);
				emu->image_header = (uint16_t*)frame;
				int ret = gd_get_frame(gif);
				if (ret != -1){
					gd_render_frame(gif, frame);
				}
			}
			gd_close_gif(gif);
		}else{
			 printf("Image Header '%s' not found\n", file);
		}
    }
}