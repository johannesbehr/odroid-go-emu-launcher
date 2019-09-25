#include "goemu_data.h"
#include "esp_partition.h"
#include "../components/odroid/odroid_settings.h"
#include <dirent.h>

#include "simplexml.h"

static const char* NvsKey_EmuOffset = "EmuOffset";

goemu_emu_data *goemu_data;

goemu_emu_data_entry *goemu_data_add(goemu_emu_data *d, const char *system, const char *path, const char* ext, const char *part, const uint16_t crc_offset, const uint16_t header_forecolor, const uint16_t header_backcolor)
{
	printf("Adding Emulator: %s, %s, %s, %s, %d\r\n", system, path, ext, part, crc_offset);
	
    goemu_emu_data_entry *p = &d->entries[d->count];
    p->nr = d->count;
    strcpy(p->partition_name, part);
    strcpy(p->system_name, system);
    strcpy(p->path, path);
    strcpy(p->path_metadata, path);
    strcpy(p->ext, ext);
    p->files.count = 0;
    p->files.buffer = NULL;
    p->files.refs = NULL;
    p->selected = 0;
    p->available = false;
    p->image_logo = NULL;
    p->image_header = NULL;
    p->initialized = false;
    p->checksums = NULL;
    p->crc_offset = crc_offset;
	p->header_forecolor = header_forecolor;
	p->header_backcolor = header_backcolor;
    d->count++;
    return p;
}

uint16_t parseColor(char* color){
	uint16_t res = 0;
	
	// RGB565
	if(strlen(color)==4){
		for(int i = 0; i< 4; i++){
			res = res << 4;
			if(color[i]>='A' && color[i]<='F'){
				res = res | (color[i] - 'A' + 0xA);
			}
			else if(color[i]>='0' && color[i]<='9'){
				res = res | (color[i] - '0');
			}
		}	
	}
	// Todo: RGB!
	
	printf("Parsed %s to: %d\r\n",color, res);
	
	return res;
	
}
	
char h_system_name[64];
char h_ext[8];
char h_path[32];
char h_partition_name[20];
uint16_t h_crc_offset = 0;
uint16_t h_header_backcolor = 0x0000;
uint16_t h_header_forecolor = 0xFFFF;

void* emulatorXmlTagHandler (SimpleXmlParser parser, SimpleXmlEvent event, const char* szName, const char* szAttribute, const char* szValue){
	
	if(strcmp(szName,"Emulator")==0){

		if (event == ADD_SUBTAG) {
			// Start of Tag
			//printf("add subtag (%s)\n",  szName);

		} else if (event == ADD_ATTRIBUTE) {
			//printf("add attribute to tag %s (%s=%s)\n", szName, szAttribute, szValue);
			
			if(!strcmp(szAttribute,"SystemName")){
				strcpy(h_system_name, szValue);
			}
			else if(!strcmp(szAttribute,"PartitionName")){
				strcpy(h_partition_name,szValue);
			}
			else if(!strcmp(szAttribute,"Path")){
				strcpy(h_path,szValue);
			}
			else if(!strcmp(szAttribute,"Extension")){
				strcpy(h_ext, szValue);
			}
			else if(!strcmp(szAttribute,"CrcOffset")){
				h_crc_offset = atoi(szValue);
			}
			else if(!strcmp(szAttribute,"HeaderBackcolor")){
				h_header_backcolor = parseColor(szValue);
			}
			else if(!strcmp(szAttribute,"HeaderForecolor")){
				h_header_forecolor = parseColor(szValue);
			}
			else{
				printf("Unknown Attribute: <%s>:<%s>\n", szAttribute, szValue);
			}
		} 
		/*
		else if (event == ADD_CONTENT) {
		printf("%6li: %s add content to tag %s (%s)\n", 
		simpleXmlGetLineNumber(parser), getIndent(nDepth), szHandlerName, szHandlerValue);
		}

		else if (event == FINISH_ATTRIBUTES) {
		printf("%6li: %s finish attributes (%s)\n", 
		simpleXmlGetLineNumber(parser), getIndent(nDepth), szHandlerName);
		}*/
		else if (event == FINISH_TAG) {
			goemu_data_add(goemu_data,h_system_name, h_path,h_ext, h_partition_name,h_crc_offset, h_header_forecolor, h_header_backcolor);	
			//printf("finish tag (%s)\n", szName);
		}

	}else{
	//	printf("szName is: (%s)\n",  szName);

	}
	return emulatorXmlTagHandler;
}


goemu_emu_data *goemu_data_setup()
{
	uint8_t max = 20;
	goemu_data = (goemu_emu_data*)malloc(sizeof(goemu_emu_data));
    goemu_data->entries = (goemu_emu_data_entry*)malloc(sizeof(goemu_emu_data_entry)*max);
    goemu_data->count = 0;
	
	parseXmlFile("/sd/odroid/metadata/launcher.xml", emulatorXmlTagHandler);

	/*
    goemu_data_add(goemu_data, "Nintendo Entertainment System", "nes", "nes", "nesemu",16);
    goemu_data_add(goemu_data, "Nintendo Gameboy", "gb", "gb", "gnuboy",0);
    goemu_data_add(goemu_data, "Nintendo Gameboy Color", "gbc", "gbc", "gnuboy",0);
    goemu_data_add(goemu_data, "Sega Master System", "sms", "sms", "smsplusgx",0);
    goemu_data_add(goemu_data, "Sega Game Gear", "gg", "gg", "smsplusgx",0);
    goemu_data_add(goemu_data, "ColecoVision", "col", "col", "smsplusgx",0);
	goemu_data_add(goemu_data, "Atari Lynx", "lnx", "lnx", "lynx",0);
    goemu_data_add(goemu_data, "Atari 2600", "a26", "a26", "stella",0);
    goemu_data_add(goemu_data, "Atari 7800", "a78", "a78", "prosystem",0);
    goemu_data_add(goemu_data, "ZX Spectrum", "spectrum", "z80", "spectrum",0);
    goemu_data_add(goemu_data, "PC Engine", "pce", "pce", "pcengine",0);
    */

	// Check if the partition to the emulator is present
    for (int i = 0; i < goemu_data->count; i++)
    {
        goemu_emu_data_entry *p = &goemu_data->entries[i];
        esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, p->partition_name);
        p->available = partition!=NULL;
    }
	
    return goemu_data;
}

goemu_emu_data_entry *goemu_data_get(const char *ext)
{
    for (int i = 0; i < goemu_data->count; i++)
    {
        goemu_emu_data_entry *p = &goemu_data->entries[i];
        if (strcmp(p->ext, ext)==0)
        {
            return p;
        }
    }
    return NULL;
}

void goemu_config_load(goemu_emu_data_entry *emu)
{
    char key[32];
    sprintf(key, "%s%d", NvsKey_EmuOffset, emu->nr);
    int32_t sel = odroid_settings_int32_get(key, -1);
    if (sel != -1) emu->selected = sel;
    
    emu->_selected = emu->selected;
    // printf("CONFIG: LOAD: %s: selected: %d\n", key, emu->selected);
}

void goemu_config_save_all()
{
    char key[32];
    for (int i = 0; i < goemu_data->count; i++)
    {
        goemu_emu_data_entry *emu = &goemu_data->entries[i];
        if (!emu->initialized || emu->_selected == emu->selected)
        {
            continue;
        }
        sprintf(key, "%s%d", NvsKey_EmuOffset, emu->nr);
        // printf("CONFIG: SAVE: %s: selected: %d\n", key, emu->selected);
        odroid_settings_int32_set(key, emu->selected);
    }
}
