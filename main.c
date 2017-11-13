/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>


unsigned int replaced_frame;
struct disk *disk;
char *policy;
unsigned int free_frame_count;
unsigned int page_fault_counter = 0;

void page_fault_handler_custom( struct page_table *pt, int page){
  int index = 0;
  int frame_idx = 0;
    
  // 1. Remove all mapping to the physical frame from virt mem
  int max_page = page + pt->nframes;
  if(max_page > pt->npages)
    max_page = pt->npages;
  for (index = page; index < max_page; index++){    
    if (pt->page_bits[index] != 0)
    {
      if (pt->page_bits[index] & PROT_WRITE)
        disk_write(disk, index, pt->physmem + pt->page_mapping[index]*PAGE_SIZE);
      page_table_set_entry(pt, index, 0, 0);
    }
  }
  
  // 2. Scan through all nframes and map the corr. virt mem starting from page
  int max_frame = pt->npages - page;
  if(max_frame > pt->nframes)
    max_frame = pt->nframes;
  for (frame_idx = 0; frame_idx < max_frame; frame_idx++){
    disk_read(disk, page+frame_idx, pt->physmem + frame_idx*PAGE_SIZE);
    page_table_set_entry(pt, page+frame_idx, frame_idx, PROT_READ);
  }  
}

void page_fault_handler( struct page_table *pt, int page )
{
  int index = 0;
  int frame_being_used = false;
  static int random_seed_flag = 0;

  //printf("page fault on page #%d\n",page);


  // check permission
  if (pt->page_bits[page] == PROT_READ) // application attemps to write into page w/o permission
  {
    page_table_set_entry(pt, page, pt->page_mapping[page], PROT_READ | PROT_WRITE);
    //printf("add WRITE permission on page #%d\n\n\n", page);
  }
  else if (pt->page_bits[page] == 0) // no frame in virt mem
  {
    page_fault_counter++;

    if (free_frame_count) // if there are free frames, no need to do any replacement
    {
      replaced_frame = pt->nframes - free_frame_count;
      free_frame_count--;
    }
    else // choose a frame to replace
    {
      if(!strcmp(policy,"fifo"))
      {
        if (++replaced_frame >= pt->nframes)
          replaced_frame = 0;
      }
      else if (!strcmp(policy,"rand"))
      {
        if (random_seed_flag == 0)
        {
          srand(time(NULL)); // only need to setup seed once
          random_seed_flag = 1;
        }
        replaced_frame = rand() % pt->nframes;
      }
      else if(!strcmp(policy,"custom")){
        page_fault_handler_custom(pt, page);
        return;
      }
      else
      {
        printf("replacement policy not implemented, program exit\n");
        exit(1);
      }
    }


    frame_being_used = false;

    // we want to replace a frame in phys mem
    // search for where the frame is being used in virt mem
    for (index = 0; index < pt->npages; index++){
      // we need to check the permission bits
      // because unused pages also have mapping number = 0, but no permission
      // this is only for situation replaced_frame = 0
      if ((pt->page_mapping[index] == replaced_frame) && (pt->page_bits[index] != 0))
      {
        frame_being_used = true;
        break;
      }
    }

    if (frame_being_used == true)
    {
      // check dirty bit
      if (pt->page_bits[index] & PROT_WRITE)
      {
        //printf("write frame %d back to disk\n", replaced_frame);
        disk_write(disk, index, pt->physmem + replaced_frame*PAGE_SIZE);
      }

      page_table_set_entry(pt, index, 0, 0);

      //printf("remove frame %d from page %d\n", replaced_frame, index);
    }

    disk_read(disk, page, pt->physmem + replaced_frame*PAGE_SIZE);
    page_table_set_entry(pt, page, replaced_frame, PROT_READ);

    //printf("Use frame %d for page %d\n\n\n", replaced_frame, page);		
  }
}

int main( int argc, char *argv[] )
{
  if(argc!=5) {
    printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
    return 1;
  }

  int npages = atoi(argv[1]);
  int nframes = atoi(argv[2]);
  const char *program = argv[4];

  policy = argv[3];
  free_frame_count = nframes;


  //struct disk *disk = disk_open("myvirtualdisk",npages);
  disk = disk_open("myvirtualdisk",npages);

  if(!disk) {
    fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
    return 1;
  }


  struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
  if(!pt) {
    fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
    return 1;
  }


  char *virtmem = page_table_get_virtmem(pt);

  //char *physmem = page_table_get_physmem(pt);

  if(!strcmp(program,"sort")) {
    sort_program(virtmem,npages*PAGE_SIZE);
  } else if(!strcmp(program,"scan")) {
    scan_program(virtmem,npages*PAGE_SIZE);
  } else if(!strcmp(program,"focus")) {
    focus_program(virtmem,npages*PAGE_SIZE);
  } else {
    fprintf(stderr,"unknown program: %s\n",argv[3]);

  }

  printf("page fault count = %d\n", page_fault_counter);

  page_table_delete(pt);
  disk_close(disk);

  return 0;
}
