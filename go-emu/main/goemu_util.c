#include "goemu_util.h"
#include "esp_system.h"
#include "esp_event.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>

int goemu_util_comparator(const void *p, const void *q) 
{ 
    // Get the values at given addresses 
    char *l = (char*)(*(uint32_t*)p); 
    char *r = (char*)(*(uint32_t*)q);
    return strcmp(l,r); 
}

void goemu_util_readdir(goemu_util_strings *rc, const char *path, const char *ext)
{
    rc->count = 0;
    rc->buffer = NULL;
    rc->refs = NULL;
    DIR* dir = opendir(path);
    if (!dir)
    {
        printf("goemu_util_readdir: failed '%s'. ERR: %d\n", path, errno);
        return;    
    }
    int count = 0;
    int count_chars = 0;
    struct dirent* in_file;
    uint32_t *entries_refs = NULL;
    char *entries_buffer = NULL;
    while (!entries_refs)
    {
    rewinddir(dir);
    if (count>0) {
       entries_refs = (uint32_t*)heap_caps_malloc(count*4, MALLOC_CAP_SPIRAM);
       entries_buffer = (char*)heap_caps_malloc(count_chars, MALLOC_CAP_SPIRAM);
       count = 0;
       count_chars = 0;
    }
	
	// Split extensions
	char extensionbuffer[100];
	// Copy string to new string
	sprintf(extensionbuffer, "%s", ext);
	int extensions[10];
	int extension_cnt = 1;
	
	extensions[0] = extensionbuffer;
	
	// replace all ; with \0
	char *pos;
	pos = extensionbuffer;
	printf("pos as string: %s\r\n",pos);
	printf("pos(1): %p\r\n",pos);
	
	while((pos=strchr(pos + 1, ';'))!=NULL){
		printf("pos(x): %p\r\n",pos);
		
		extensions[extension_cnt]=pos+1;
		*pos = '\0';
		extension_cnt++;
	}
	
	printf("Found %d extensions: \r\n",extension_cnt);
	for(int ii = 0; ii<extension_cnt; ii++){
		printf("%s\r\n", (char*)extensions[ii]);
	}
	
    while ((in_file = readdir(dir))) 
    {
        if (!strcmp (in_file->d_name, "."))
            continue;
        if (!strcmp (in_file->d_name, ".."))    
            continue;
        if (strlen(in_file->d_name) < strlen(ext)+1)
            continue;
        if (in_file->d_name[0] == '.' && in_file->d_name[1] == '_')    
            continue;
		
		// Search ';' in ext
		// Split at it
		
		// For each extension...
		
			bool found = 0;
			char* ext2;
	for(int ii = 0; ii<extension_cnt; ii++){
		ext2 = (char*)extensions[ii];
		if(strcasecmp(ext2, &in_file->d_name[strlen(in_file->d_name)-strlen(ext2)]) == 0){
			found = 1;
		}
	}
		
		
	if(found == 0) continue;
//        if (strcasecmp(ext, &in_file->d_name[strlen(in_file->d_name)-strlen(ext)]) != 0)
 //           continue;
        
		
		
		
        if (entries_refs) {
           entries_refs[count] = ((uint32_t)entries_buffer) + count_chars;
           strcpy(&entries_buffer[count_chars],in_file->d_name);
           //printf("DIR entry: %p; %d; Offset: %d; File: %s\n", (char*)entries_refs[count], count, count_chars, in_file->d_name);
        }
        count++;
        count_chars+=strlen(in_file->d_name)+1;
    }
      if (count == 0) {
        break;
      }
    }
    closedir(dir);
    
    if (!entries_refs) {
       printf("Could not find any files: %s\n", path);
       return NULL;
    }
    rc->buffer = entries_buffer;
    rc->refs = entries_refs;
    rc->count = count;
}

void goemu_util_strings_sort(goemu_util_strings *data)
{
    qsort((void*)data->refs, data->count, sizeof(data->refs[0]), goemu_util_comparator); 
}

void goemu_util_strings_free(goemu_util_strings *data)
{
    if (data->refs) heap_caps_free(data->refs);
    if (data->buffer) heap_caps_free(data->buffer);
    data->count = 0;
    data->refs = NULL;
    data->buffer = NULL;
}
