/*
 * $Id: VMMethod.c 227 2008-04-21 15:21:14Z michael.haupt $
 *
Copyright (c) 2007 Michael Haupt, Tobias Pape
Software Architecture Group, Hasso Plattner Institute, Potsdam, Germany
http://www.hpi.uni-potsdam.de/swa/

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
  */
 
#include "VMMethod.h"
#include "VMObject.h"
#include "VMSymbol.h"
#include "Signature.h"
#include "VMInteger.h"
#include "VMInvokable.h"

#include <memory/gc.h>

#include <vm/Universe.h>

#include <interpreter/bytecodes.h>
#include <interpreter/Interpreter.h>

#include <misc/debug.h>

#include <compiler/GenerationContexts.h>

#include <stdbool.h>
#include <stddef.h>


//
//  Class Methods (Starting with VMMethod_) 
//


/**
 * Create a new VMMethod
 */
pVMMethod VMMethod_new(size_t number_of_bytecodes, size_t number_of_constants) {
    pVMMethod result = (pVMMethod)gc_allocate_object(
        sizeof(VMMethod) + (sizeof(pVMObject) * number_of_constants) +
        (sizeof(uint8_t) * number_of_bytecodes));
    if(result) {
        result->_vtable = VMMethod_vtable();
		gc_start_uninterruptable_allocation();
        INIT(result, number_of_constants, number_of_bytecodes);
        gc_end_uninterruptable_allocation();
    }
    return result;
}


pVMMethod VMMethod_assemble(method_generation_context* mgenc) {
    // create a method instance with the given number of bytecodes and literals
    int num_literals = SEND(mgenc->literals, size);
    pVMMethod meth = Universe_new_method(mgenc->signature, mgenc->bp,
        SEND(mgenc->literals, size));
    
    // populate the fields that are immediately available
    int num_locals = SEND(mgenc->locals, size);
    SEND(meth, set_number_of_locals, num_locals);
    SEND(meth, set_maximum_number_of_stack_elements,
        method_genc_compute_stack_depth(mgenc));
    
    // copy literals into the method
    for(int i = 0; i < num_literals; i++) {
        pVMObject l = SEND(mgenc->literals, get, i);
        SEND(meth, set_indexable_field, i, l);
    }
    
    // copy bytecodes into method
    for(size_t i = 0; i < mgenc->bp; i++)
        SEND(meth, set_bytecode, i, mgenc->bytecode[i]);
    
    
    // return the method - the holder field is to be set later on!
    return meth;
}


/**
 * Initialize a VMMethod
 */
void _VMMethod_init(void* _self, ...) {
    pVMMethod self = (pVMMethod)_self;

    va_list args;
    va_start(args, _self);
    
    SUPER(VMArray, _self, init, va_arg(args,size_t));
    self->bytecodes_length= Universe_new_integer(va_arg(args,size_t));
    va_end(args);
    
    self->number_of_arguments = NULL;
    self->number_of_locals = NULL;
}


void _VMMethod_free(void* _self) {
    pVMMethod self = (pVMMethod)_self;

    if(self->bytecodes_length)
        SEND(self->bytecodes_length, free);
    if(self->maximum_number_of_stack_elements)
        SEND(self->maximum_number_of_stack_elements, free);
    if(self->number_of_locals)
        SEND(self->number_of_locals, free);
    if(self->number_of_arguments)
        SEND(self->number_of_arguments, free);

    SUPER(VMArray, self, free);
}


/**
 *
 * Return the offset of the indexable Fields from "normal" fields
 *
 */
size_t _VMMethod__get_offset(void* _self) {
    return (size_t)SIZE_DIFF_VMOBJECT(VMMethod);
}


//
//  Instance Methods (Starting with _VMMethod_) 
//

int _VMMethod_get_number_of_locals(void* _self) {
    pVMMethod self = (pVMMethod)_self;
    // Get the number of locals
    return SEND(self->number_of_locals, get_embedded_integer);
}


void _VMMethod_set_number_of_locals(void* _self, int value) {
    pVMMethod self = (pVMMethod)_self;
    // Set the number of locals
    if(!self->number_of_locals)
        self->number_of_locals = Universe_new_integer(value);
}


void _VMMethod_set_number_of_arguments(void* _self, int value) {
    pVMMethod self = (pVMMethod)_self;
    // Set the number of arguments
    if(!self->number_of_arguments)
        self->number_of_arguments = Universe_new_integer(value);
}


int _VMMethod_get_maximum_number_of_stack_elements(void* _self) {
    pVMMethod self = (pVMMethod)_self;
    // Get the max. number of Stack elements as int
    return SEND(self->maximum_number_of_stack_elements, get_embedded_integer);
}


void _VMMethod_set_maximum_number_of_stack_elements(void* _self, int value) {
    pVMMethod self = (pVMMethod)_self;
    // Set the max. number of Stack elements from value
    self->maximum_number_of_stack_elements = Universe_new_integer(value);
}

void _VMMethod_set_holder_all(void* _self, pVMClass value) {
    pVMMethod self = (pVMMethod)_self;
    // Make sure all nested invokables have the same holder
    for(size_t i = 0; 
        i < SEND(self, get_number_of_indexable_fields);
        i++) {
        pVMObject o = SEND(self, get_indexable_field, i);
        
        if(SUPPORTS(o, VMInvokable))
            TSEND(VMInvokable, o, set_holder, value);
    }
}

pVMObject _VMMethod_get_constant(void* _self, int bytecode_index) {
    pVMMethod self = (pVMMethod)_self;
    uint8_t bc = SEND(self, get_bytecode, bytecode_index + 1);
    return SEND((pVMArray)self, get_indexable_field, bc);
}


int _VMMethod_get_number_of_arguments(void* _self) {
    pVMMethod self = (pVMMethod)_self;
    return SEND(self->number_of_arguments, get_embedded_integer);
}


int _VMMethod_get_number_of_bytecodes(void* _self) {
    pVMMethod self = (pVMMethod)_self;
    if(self->bytecodes_length)
        return SEND(self->bytecodes_length, get_embedded_integer);
    else  // no integer there
        return -1;
}


uint8_t _VMMethod_get_bytecode(void* _self, int index) {
    pVMMethod self = (pVMMethod)_self;
    #ifdef DEBUG
        if(index >= SEND(self->bytecodes_length, get_embedded_integer))
            Universe_error_exit("[get] Method Bytecode Index out of range.");
    #endif // DEBUG
    /*
     * Bytecodes start at end of the internal array,
     * thus, at offset + number_of_constants
     */
    return ((uint8_t*)&(self->fields[
        SEND(self, _get_offset) + 
        SEND(self, get_number_of_indexable_fields)
    ]))[index];
}


void _VMMethod_set_bytecode(void* _self, int index, uint8_t value) {
    pVMMethod self = (pVMMethod)_self;
    #ifdef DEBUG
        if(index >= SEND(self->bytecodes_length, get_embedded_integer))
            Universe_error_exit("[set] Method Bytecode Index out of range.");  
    #endif // DEBUG
    /*
     * Bytecodes start at end of the internal array,
     * thus, at offset + number_of_constants
     */        
    ((uint8_t*)&(self->fields[
        SEND(self, _get_offset) + 
        SEND(self, get_number_of_indexable_fields)
    ]))[index] = value;
}


void _VMMethod_invoke_method(void* _self, pVMFrame frame) {
    pVMMethod self = (pVMMethod)_self;
    //// Increase the invocation counter
    //self->invocation_count++;
    // Allocate and push a new frame on the interpreter stack
    pVMFrame frm = Interpreter_push_new_frame(self);    
    SEND(frm, copy_arguments_from, frame);
}


void _VMMethod_mark_references(void* _self) {
    pVMMethod self = (pVMMethod) _self;
    gc_mark_object(self->signature);
    gc_mark_object(self->holder); 
    gc_mark_object(self->number_of_locals);
    gc_mark_object(self->maximum_number_of_stack_elements);
    gc_mark_object(self->bytecodes_length);
    gc_mark_object(self->number_of_arguments);
	SUPER(VMArray, self, mark_references);
}


//
// The VTABLE-function
//


static VTABLE(VMMethod) _VMMethod_vtable;
bool VMMethod_vtable_inited = false;


VTABLE(VMMethod)* VMMethod_vtable(void) {
    if(! VMMethod_vtable_inited) {
        *((VTABLE(VMArray)*)&_VMMethod_vtable) = *VMArray_vtable();
        
        ASSIGN_TRAIT(VMInvokable, VMMethod);

        _VMMethod_vtable.init = METHOD(VMMethod, init);        
        _VMMethod_vtable.free = METHOD(VMMethod, free);
        _VMMethod_vtable.get_number_of_locals =
            METHOD(VMMethod, get_number_of_locals);
        _VMMethod_vtable.set_number_of_locals =
            METHOD(VMMethod, set_number_of_locals);
        _VMMethod_vtable.get_maximum_number_of_stack_elements =
            METHOD(VMMethod, get_maximum_number_of_stack_elements);
        _VMMethod_vtable.set_maximum_number_of_stack_elements =
            METHOD(VMMethod, set_maximum_number_of_stack_elements);
        _VMMethod_vtable._get_offset = METHOD(VMMethod, _get_offset);
        _VMMethod_vtable.set_holder_all = METHOD(VMMethod, set_holder_all);
        _VMMethod_vtable.get_constant = METHOD(VMMethod, get_constant);
        _VMMethod_vtable.get_number_of_arguments =
            METHOD(VMMethod, get_number_of_arguments);
        _VMMethod_vtable.set_number_of_arguments =
            METHOD(VMMethod, set_number_of_arguments);
        _VMMethod_vtable.get_number_of_bytecodes =
            METHOD(VMMethod, get_number_of_bytecodes);
        _VMMethod_vtable.get_bytecode = METHOD(VMMethod, get_bytecode);
        _VMMethod_vtable.set_bytecode = METHOD(VMMethod, set_bytecode);
        _VMMethod_vtable.invoke_method = METHOD(VMMethod, invoke_method);
        
        _VMMethod_vtable.mark_references = 
            METHOD(VMMethod, mark_references);

        VMMethod_vtable_inited = true;
    }
    return &_VMMethod_vtable;
}
