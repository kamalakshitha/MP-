/*
 File: ContFramePool.C
 
 Author: Kamalakshitha Aligeri
 Date  : 09/12/2019
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "utils.H"
#include "console.H"
#include "cont_frame_pool.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/
//ContFramePool* ContFramePool::frame_pool_list_head;
ContFramePool* ContFramePool::frame_pool_list;

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no,
                             unsigned long _n_info_frames)
{
    assert(_n_frames <= FRAME_SIZE * 4);

    // Initialize variables
    info_frame_number = _info_frame_no;
    nFreeFrames = _n_frames;
    nframes = _n_frames;
    base_frame_number = _base_frame_no;

    // If _info_frame_no is zero then we keep management info in the first
    //frame, else we use the provided frame to keep management info
    if(info_frame_number == 0) {
        bitmap = (unsigned char *) (base_frame_number * FRAME_SIZE);
    } else {
        bitmap = (unsigned char *) (info_frame_number * FRAME_SIZE);
    }

    assert ((nframes % 4 ) == 0); // Check if the number of frames is a multiple of 4


    // Mark all frames as free
    for(int i=0; i*4 < _n_frames; i++) 
        bitmap[i] = 0xFF;
    

    // Mark the first frame as allocated. 00
    if(info_frame_number == 0) {
        bitmap[0] = 0x3F;
        nFreeFrames--;
    }

    

     // Store the pointer into the list. This helps in identifying kernel frame
     // pool and process frame pool
      if (frame_pool_list==NULL) {
         frame_pool_list=this;
         this->next = NULL;
         this->prev = NULL;
     } else {
         this->prev = frame_pool_list;
         this->next = frame_pool_list->next;
         if (this->next != NULL) {
           this->next->prev = this;
         }
         frame_pool_list->next = this;
     }


    Console::puts("Frame Pool initialized\n");

}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    unsigned int  i = 0, j = 0, k = 0;
    unsigned int  head_index = 0;
    unsigned int  count = 0;
    unsigned int  index = 0;
    unsigned int  first_frame_no = 0;
    unsigned int  last_frame_no = 0;
    unsigned char rest_mask = 0xC0;
    unsigned char mask = 0xC0;
    unsigned int  frame_no = base_frame_number;
    unsigned int  requested_frames = _n_frames;

    if(requested_frames > nFreeFrames) {
        Console::puts("Frames not available");
        Console::puts("FreeFrames = "); Console::puti(nFreeFrames);Console::puts("\n");
        Console::puts("_n_frames = "); Console::puti(_n_frames);Console::puts("\n");
    }

    // loop through all frames
    while (i < (nframes / 4) && (count < requested_frames))
    {
        // check bits in a bitmap[i] as one byte in bitmap stores 4 frames
        for (j = 0; j < 4 && (count < requested_frames); j++)
        {
            if ((bitmap[i] & mask) == mask)
            {
                // free frame, increase count
                count++;
                last_frame_no = base_frame_number + i*4 + j;
            }
            else count = 0;
             mask = mask >> 2;
        }
        mask = 0xC0;
        i++;
    }

    first_frame_no = last_frame_no - requested_frames + 1;

    // get the mask for head frame
    rest_mask = 0xC0 >> ((first_frame_no%4) * 2);
    mask = 0xBF >> ((first_frame_no%4) * 2);

    // marking first frame as head frame (10) in bit map
    unsigned long bitmap_index = (first_frame_no - base_frame_number)/4;
    bitmap[bitmap_index] = (bitmap[bitmap_index] & (~rest_mask)) | mask;

    // marking rest of frames as allocated frame (00) in bit map
    for (k = first_frame_no + 1; k <= last_frame_no; k++)
    {
        index = (k - base_frame_number)/4;
        mask = 0xC0 >> ((k%4)*2);
        bitmap[index] = bitmap[index] & (~mask);
    }

    nFreeFrames -= requested_frames;
    return (first_frame_no);


}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    unsigned char mask, rest_mask;
    int k, index;
    unsigned long last_inaccess_frame  = _base_frame_no + _n_frames - 1;

    for (k = _base_frame_no; k <= last_inaccess_frame; k++)
    {
        // find the index and mark it as head frame (10)
        index = (k - _base_frame_no)/4;
        rest_mask = 0xC0 >> ((k%4)*2);
        mask = 0xBF >> ((k%4) * 2);
        bitmap[index] = (bitmap[index] & (~rest_mask))| mask;
    }

}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    unsigned int   first_frame_no = _first_frame_no;
    unsigned int   next_frame_no = 0;
    unsigned int   i = 0, j = 0, k = 0;
    unsigned int   frame_free_count = 0;
    unsigned char  mask_free;
    unsigned char  mask;
    unsigned char  mask_get_head_frame;
    unsigned char  mask_check_head_frame;
    unsigned char* bitmap;
    unsigned int   nFreeFrames;
    unsigned long  base_frame_number;
    unsigned long  nframes;
    unsigned long  info_frame_number;
    unsigned int   head_index = 0;

    
    //determine if frame is in kernel pool or process pool
    // Find the pool to which this frame belongs
    ContFramePool* curr = frame_pool_list;
    while ( (curr->base_frame_number > first_frame_no || curr->base_frame_number + curr->nframes <= first_frame_no) ) {
      if (curr->next == NULL) {
        Console::puts("Frame not found in any pool, cannot release. \n");
        return;
      } else {
        curr = curr->next;
      }
    }


    // Assign initial variables
    base_frame_number = curr->base_frame_number;
    nframes = curr->nframes;
    info_frame_number = curr->info_frame_number;
    bitmap = curr->bitmap;
    nFreeFrames = curr->nFreeFrames;

    mask = 0x80 >> ((first_frame_no%4) * 2);
    mask_get_head_frame = 0xC0 >> ((first_frame_no%4) * 2);
    mask_check_head_frame = 0x80 >> ((first_frame_no%4) * 2);

    head_index = bitmap[(first_frame_no - base_frame_number)/4] & mask_get_head_frame;
    // check if the first frame is head frame
    if (((head_index ^ mask_check_head_frame) != 0))
    {
        Console::puts("first frame is not a head frame. Exit ");
        assert(false);
    }
    // mark the first frame as free (11)
    mask = 0xC0 >> ((first_frame_no%4) * 2);
    bitmap[(first_frame_no - base_frame_number)/4] = bitmap[(first_frame_no - base_frame_number)/4] | mask;

    // start from next frame
    next_frame_no = first_frame_no + 1;
    mask = 0xC0 >> ((next_frame_no%4) * 2);
    frame_free_count++;

    i = (next_frame_no - base_frame_number)/4;
    while (i < (nframes / 4))
    {
        // mark frames as free (11) if it is allocated (00)
        i = (next_frame_no - base_frame_number)/4;
        mask = 0xC0 >> ((next_frame_no%4) * 2);
        if ((bitmap[i] & mask) == 0)
        {
            bitmap[i] = bitmap[i] | mask;
            frame_free_count++;
        }
        else
        {
            // break the loop if the frame is free or head frame
            break;
        }
        next_frame_no++;
    }

    nFreeFrames += frame_free_count;


}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
     return _n_frames / (16*1024) + (_n_frames % (16*1024) > 0 ? 1 : 0);
}
