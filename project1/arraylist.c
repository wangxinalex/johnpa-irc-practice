/* gok-predictor.c
*
* Copyright 2002 Sun Microsystems, Inc.,
* Copyright 2002 University Of Toronto
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Library General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Library General Public License for more details.
*
* You should have received a copy of the GNU Library General Public
* License along with this library; if not, write to the
* Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA.
*/

#include <string.h>
#include "arraylist.h"

/*
  constants
*/
#define ARRAYLIST_INITIAL_CAPACITY 10
#define ARRAYLIST_CAPACITY_DELTA 10

static const size_t object_size = sizeof(Object);


/*
  structures
*/
struct Arraylist_Struct {
    int _current_capacity;
    Object *_data;
    int _size;
    //const Boolean (*_equals)();
};

void arraylist_free(const Arraylist list)
{
    free(list->_data);
    free(list);
}

Arraylist arraylist_create()//const Boolean (*equals)(const Object object_1, const Object object_2))
{
    Arraylist list;
    list = malloc(sizeof(struct Arraylist_Struct));
    if (!list)
         return NULL;
    list->_current_capacity = ARRAYLIST_INITIAL_CAPACITY;
    list->_data = malloc(object_size * list->_current_capacity);
    if (!list->_data){
         free(list);
         return NULL;
    }
    list->_size = 0;
    //list->_equals = equals;


    return list;
}

/* modified by J.W. Park to return inserted index, and -1 on error. */
int arraylist_add(const Arraylist list, Object object)
{
    int old_size = arraylist_size(list);
    int new_capacity;
    Object *new_data;

    (list->_size)++;
    if (old_size == list->_current_capacity)
    {
        new_capacity = list->_current_capacity + ARRAYLIST_CAPACITY_DELTA;
        new_data = malloc(object_size * new_capacity);
        if (!new_data){
            return -1;
        }
        memcpy(new_data, list->_data, object_size * old_size);
        free(list->_data);
        (list->_data) = new_data;
        list->_current_capacity = new_capacity;
    }
    (list->_data)[old_size] = object;
    return old_size;
}

Object arraylist_remove(const Arraylist list, const Object object)
{
    int index = arraylist_index_of(list, object);
    return arraylist_removeIndex(list,index);

    /*
    int length = arraylist_size(list);
    int last_index = length - 1;
    int new_size, new_capacity;
    int index;

    for (index = 0; index < length; index++)
    {
        if ((*list->_equals)(arraylist_get(list, index), object))
        {
            (list->_size)--;
            if (index < last_index)
            {
                memmove(list->_data + index, list->_data + index + 1, object_size * (last_index - index));
                new_size = list->_size;
                new_capacity = list->_current_capacity - ARRAYLIST_CAPACITY_DELTA;
                if (new_capacity > new_size)
                {
                    list->_data = realloc(list->_data, object_size * new_capacity);
                    list->_current_capacity = new_capacity;
                }
            }
            return TRUE;
        }
    }
    return FALSE; */
}
/* added by J.W. Park */
Object arraylist_removeIndex(const Arraylist list, const int index){
  int length = arraylist_size(list);
  int last_index = length - 1;
  int new_size, new_capacity;
  Object toRemove = NULL;

  if (index < 0 || index > arraylist_size(list))
    return NULL;

  (list->_size)--;
  toRemove = list->_data + index;

  if (index < last_index)
  {
    memmove(list->_data + index, list->_data + index + 1, object_size * (last_index - index));
    new_size = list->_size;
    new_capacity = list->_current_capacity - ARRAYLIST_CAPACITY_DELTA;
    if (new_capacity > new_size)
    {
      list->_data = realloc(list->_data, object_size * new_capacity);
      list->_current_capacity = new_capacity;
    }
  }

  return toRemove;
}
Boolean arraylist_contains(const Arraylist list, const Object object)
{
    return (arraylist_index_of(list, object) > -1);
}

int arraylist_index_of(const Arraylist list, const Object object)
{
    int length = arraylist_size(list);
    int index;

    for (index = 0; index < length; index++)
    {
        /* we will only use shallow copies */
        //if ((*list->_equals)(arraylist_get(list, index), object))
        if( arraylist_get(list, index) == object)
        {
            return index;
        }
    }
    return -1;
}

Boolean arraylist_is_empty(const Arraylist list)
{
    return (0 == arraylist_size(list));
}

int arraylist_size(const Arraylist list)
{
    return list->_size;
}

Object arraylist_get(const Arraylist list, const int index)
{
    return list->_data[index];
}

void arraylist_clear(const Arraylist list)
{
    list->_data = realloc(list->_data, object_size * ARRAYLIST_INITIAL_CAPACITY);
    list->_current_capacity = ARRAYLIST_INITIAL_CAPACITY;
    list->_size = 0;
}

void arraylist_sort(const Arraylist list, const int (*compare)(const Object object_1, const Object object_2))
{
    qsort(list->_data,
            arraylist_size(list),
            sizeof(Object),
            (int (*)())compare);
}
