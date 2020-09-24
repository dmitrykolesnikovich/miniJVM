

#include "../utils/d_type.h"
#include "jvm.h"
#include "jvm_util.h"
#include "jit.h"


/* ==================================opcode implementation =============================*/




void _op_notsupport(u8 *opCode, Runtime *runtime) {
    invoke_deepth(runtime);
    jvm_printf("not support instruct [%x]\n", opCode[0]);
    exit(2);
}

//----------------------------------  tool func  ------------------------------------------


s32 exception_handle(RuntimeStack *stack, Runtime *runtime) {

    Instance *ins = pop_ref(stack);
    CodeAttribute *ca = runtime->method->converted_code;

#if _JVM_DEBUG_BYTECODE_DETAIL > 3
    JClass *clazz = runtime->clazz;
    s32 lineNum = getLineNumByIndex(runtime->method->converted_code, (s32) (runtime->pc - runtime->method->converted_code->code));
    jvm_printf("Exception   at %s.%s(%s.java:%d)\n",
               utf8_cstr(clazz->name), utf8_cstr(runtime->method->name),
               utf8_cstr(clazz->name),
               lineNum
    );
#endif
    s32 index = 0;
    ExceptionTable *et = NULL;// _find_exception_handler(runtime, ins, ca, (s32) (ip - ca->code), &index);
    s32 offset = (s32) (runtime->pc - ca->code);
    s32 i;
    ExceptionTable *e = ca->exception_table;
    for (i = 0; i < ca->exception_table_length; i++) {

        if (offset >= (e + i)->start_pc
            && offset <= (e + i)->end_pc) {
            if (!(e + i)->catch_type) {
                et = e + i;
                index = i;
                break;
            }
            ConstantClassRef *ccr = class_get_constant_classref(runtime->clazz, (e + i)->catch_type);
            JClass *catchClass = classes_load_get(ccr->name, runtime);
            if (instance_of(ins, catchClass)) {
                et = e + i;
                index = i;
                break;
            }
        }
    }
    if (et == NULL) {
        localvar_dispose(runtime);
        push_ref(stack, ins);
        return 0;
    } else {
        push_ref(stack, ins);
#if _JVM_DEBUG_BYTECODE_DETAIL > 3
        jvm_printf("Exception : %s\n", utf8_cstr(ins->mb.clazz->name));
#endif
        runtime->pc = (ca->code + et->handler_pc);
        jit_set_exception_jump_addr(runtime, ca, index);
        return 1;
    }

}

s32 _jarray_check_exception(Instance *arr, s32 index, Runtime *runtime) {
    if (!arr) {
        Instance *exception = exception_create(JVM_EXCEPTION_NULLPOINTER, runtime);
        push_ref(runtime->stack, (__refer) exception);
    } else if (index >= arr->arr_length || index < 0) {
        Instance *exception = exception_create(JVM_EXCEPTION_ARRAYINDEXOUTOFBOUNDS, runtime);
        push_ref(runtime->stack, (__refer) exception);
    } else {
        return RUNTIME_STATUS_NORMAL;
    }
    return RUNTIME_STATUS_EXCEPTION;
}

void _outofbounds_throw_exception(RuntimeStack *stack, Runtime *runtime) {
    Instance *exception = exception_create(JVM_EXCEPTION_ARRAYINDEXOUTOFBOUNDS, runtime);
    push_ref(runtime->stack, (__refer) exception);
}

void _null_throw_exception(RuntimeStack *stack, Runtime *runtime) {
    Instance *exception = exception_create(JVM_EXCEPTION_NULLPOINTER, runtime);
    push_ref(stack, (__refer) exception);
}

void _nosuchmethod_check_exception(c8 *mn, RuntimeStack *stack, Runtime *runtime) {
    Instance *exception = exception_create_str(JVM_EXCEPTION_NOSUCHMETHOD, runtime, mn);
    push_ref(stack, (__refer) exception);
}

void _nosuchfield_check_exception(c8 *mn, RuntimeStack *stack, Runtime *runtime) {
    Instance *exception = exception_create_str(JVM_EXCEPTION_NOSUCHFIELD, runtime, mn);
    push_ref(stack, (__refer) exception);
}

void _arrithmetic_throw_exception(RuntimeStack *stack, Runtime *runtime) {
    Instance *exception = exception_create(JVM_EXCEPTION_ARRITHMETIC, runtime);
    push_ref(stack, (__refer) exception);
}

void _checkcast_throw_exception(RuntimeStack *stack, Runtime *runtime) {
    Instance *exception = exception_create(JVM_EXCEPTION_CLASSCAST, runtime);
    push_ref(stack, (__refer) exception);
}


static s32 filterClassName(Utf8String *clsName) {
    if (utf8_indexof_c(clsName, "com/sun") < 0
        && utf8_indexof_c(clsName, "java/") < 0
        && utf8_indexof_c(clsName, "javax/") < 0) {
        return 1;
    }
    return 0;
}

#define check_gc_pause(offset){\
    if (offset < 0 && threadInfo->suspend_count) {\
        runtime->pc = ip;\
        runtime->stack->sp = sp;\
        if (threadInfo->is_interrupt) {\
            goto label_exit_while;\
        }\
        check_suspend_and_pause(runtime);\
    }\
}

s32 invokedynamic_prepare(Runtime *runtime, BootstrapMethod *bootMethod, ConstantInvokeDynamic *cid) {
    // =====================================================================
    //         run bootstrap method java.lang.invoke.LambdaMetafactory
    //
    //         public static CallSite metafactory(
    //                   MethodHandles.Lookup caller,
    //                   String invokedName,
    //                   MethodType invokedType,
    //                   MethodType samMethodType,
    //                   MethodHandle implMethod,
    //                   MethodType instantiatedMethodType
    //                   )
    //
    //          to generate Lambda Class implementation specify interface
    //          and new a callsite
    // =====================================================================
    JClass *clazz = runtime->clazz;
    RuntimeStack *stack = runtime->stack;

    //parper bootMethod parameter
    Instance *lookup = method_handles_lookup_create(runtime, clazz);
    push_ref(stack, lookup); //lookup

    Utf8String *ustr_invokeName = class_get_constant_utf8(clazz, class_get_constant_name_and_type(clazz, cid->nameAndTypeIndex)->nameIndex)->utfstr;
    Instance *jstr_invokeName = jstring_create(ustr_invokeName, runtime);
    push_ref(stack, jstr_invokeName); //invokeName

    Utf8String *ustr_invokeType = class_get_constant_utf8(clazz, class_get_constant_name_and_type(clazz, cid->nameAndTypeIndex)->typeIndex)->utfstr;
    Instance *mt_invokeType = method_type_create(runtime, ustr_invokeType);
    push_ref(stack, mt_invokeType); //invokeMethodType

    //other bootMethod parameter

    s32 i;
    for (i = 0; i < bootMethod->num_bootstrap_arguments; i++) {
        ConstantItem *item = class_get_constant_item(clazz, bootMethod->bootstrap_arguments[i]);
        switch (item->tag) {
            case CONSTANT_METHOD_TYPE: {
                ConstantMethodType *cmt = (ConstantMethodType *) item;
                Utf8String *arg = class_get_constant_utf8(clazz, cmt->descriptor_index)->utfstr;
                Instance *mt = method_type_create(runtime, arg);
                push_ref(stack, mt);
                break;
            }
            case CONSTANT_STRING_REF: {
                ConstantStringRef *csr = (ConstantStringRef *) item;
                Utf8String *arg = class_get_constant_utf8(clazz, csr->stringIndex)->utfstr;
                Instance *spec = jstring_create(arg, runtime);
                push_ref(stack, spec);
                break;
            }
            case CONSTANT_METHOD_HANDLE: {
                ConstantMethodHandle *cmh = (ConstantMethodHandle *) item;
                MethodInfo *mip = find_methodInfo_by_methodref(clazz, cmh->reference_index, runtime);
                Instance *mh = method_handle_create(runtime, mip, cmh->reference_kind);
                push_ref(stack, mh);
                break;
            }
            default: {
                jvm_printf("invokedynamic para parse error.");
            }
        }

    }

    //get bootmethod
    MethodInfo *boot_m = find_methodInfo_by_methodref(clazz, class_get_method_handle(clazz, bootMethod->bootstrap_method_ref)->reference_index, runtime);
    if (!boot_m) {
        ConstantMethodRef *cmr = class_get_constant_method_ref(clazz, class_get_method_handle(clazz, bootMethod->bootstrap_method_ref)->reference_index);
        _nosuchmethod_check_exception(utf8_cstr(cmr->name), stack, runtime);
        return RUNTIME_STATUS_EXCEPTION;
    } else {
        s32 ret = execute_method_impl(boot_m, runtime);
        if (ret == RUNTIME_STATUS_NORMAL) {
            MethodInfo *finder = find_methodInfo_by_name_c("org/mini/reflect/vm/LambdaUtil",
                                                           "getMethodInfoHandle",
                                                           "(Ljava/lang/invoke/CallSite;)J",
                                                           runtime);
            if (!finder) {
                _nosuchmethod_check_exception("getMethodInfoHandle", stack, runtime);
                return RUNTIME_STATUS_EXCEPTION;
            } else {
                //run finder to convert calsite.target(MethodHandle) to MethodInfo * pointer
                ret = execute_method_impl(finder, runtime);
                if (ret == RUNTIME_STATUS_NORMAL) {
                    MethodInfo *make = (MethodInfo *) (intptr_t) pop_long(stack);
                    bootMethod->make = make;
                }
            }
        } else {
            return ret;
        }
    }
    return RUNTIME_STATUS_NORMAL;
}

s32 checkcast(Runtime *runtime, Instance *ins, s32 typeIdx) {

    JClass *clazz = runtime->clazz;
    if (ins != NULL) {
        if (ins->mb.type == MEM_TYPE_INS) {
            JClass *other = getClassByConstantClassRef(clazz, typeIdx, runtime);
            if (instance_of(ins, other)) {
                return 1;
            }
        } else if (ins->mb.type == MEM_TYPE_ARR) {
            Utf8String *utf = class_get_constant_classref(clazz, typeIdx)->name;
            u8 ch = utf8_char_at(utf, 1);
            if (getDataTypeIndex(ch) == ins->mb.clazz->mb.arr_type_index) {
                return 1;
            }
        } else if (ins->mb.type == MEM_TYPE_CLASS) {
            Utf8String *utf = class_get_constant_classref(clazz, typeIdx)->name;
            if (utf8_equals_c(utf, STR_CLASS_JAVA_LANG_CLASS)) {
                return 1;
            }
        }
    } else {
        return 1;
    }
    return 0;
}

/**
* 把堆栈中的方法调用参数存入方法本地变量
* 调用方法前，父程序把函数参数推入堆栈，方法调用时，需要把堆栈中的参数存到本地变量
* @param method  method
* @param father  runtime of father
* @param son     runtime of son
*/



static inline void _synchronized_lock_method(MethodInfo *method, Runtime *runtime) {
    //synchronized process
    if (!jdwpserver.thread_sync_ignore) {
        if (method->is_static) {
            runtime->lock = (MemoryBlock *) runtime->clazz;
        } else {
            runtime->lock = (MemoryBlock *) localvar_getRefer(runtime->localvar, 0);
        }
        jthread_lock(runtime->lock, runtime);
    }
}

static inline void _synchronized_unlock_method(MethodInfo *method, Runtime *runtime) {
    //synchronized process
    if (!jdwpserver.thread_sync_ignore) {
        jthread_unlock(runtime->lock, runtime);
        runtime->lock = NULL;
    }
}

/**
 *    only static and special can be optimize , invokevirtual and invokeinterface may called by diff instance
 * @param subm
 * @param parent_method_code
 */
static inline void _optimize_empty_method_call(MethodInfo *subm, CodeAttribute *parent_ca, u8 *parent_method_code) {
    CodeAttribute *ca = subm->converted_code;
    u8 *parent_jit_code = &parent_ca->bytecode_for_jit[parent_method_code - parent_ca->code];
    if (ca && ca->code_length == 1 && *ca->code == op_return) {//empty method, do nothing
        s32 paras = subm->para_slots;//

        if (paras == 0) {
            *parent_method_code = op_nop;
            *(parent_method_code + 1) = op_nop;
            *(parent_method_code + 2) = op_nop;

            *parent_jit_code = op_nop;
            *(parent_jit_code + 1) = op_nop;
            *(parent_jit_code + 2) = op_nop;
        } else if (paras == 1) {
            *parent_method_code = op_pop;
            *(parent_method_code + 1) = op_nop;
            *(parent_method_code + 2) = op_nop;

            *parent_jit_code = op_pop;
            *(parent_jit_code + 1) = op_nop;
            *(parent_jit_code + 2) = op_nop;
        } else if (paras == 2) {
            *parent_method_code = op_pop2;
            *(parent_method_code + 1) = op_nop;
            *(parent_method_code + 2) = op_nop;

            *parent_jit_code = op_pop2;
            *(parent_jit_code + 1) = op_nop;
            *(parent_jit_code + 2) = op_nop;
        }
    }
}


static inline int _optimize_inline_getter(JClass *clazz, s32 cmrIdx, Runtime *runtime) {

    RuntimeStack *stack = runtime->stack;
    FieldInfo *fi = class_get_constant_fieldref(clazz, cmrIdx)->fieldInfo;
    if (!fi) {
        ConstantFieldRef *cfr = class_get_constant_fieldref(clazz, cmrIdx);
        fi = find_fieldInfo_by_fieldref(clazz, cfr->item.index, runtime);
        cfr->fieldInfo = fi;
        if (!fi) {
            _nosuchfield_check_exception(utf8_cstr(cfr->name), runtime->stack, runtime);
            return RUNTIME_STATUS_EXCEPTION;
        }
    }
    Instance *fins = pop_ref(stack);
    if (!fins) {
        _null_throw_exception(stack, runtime);
        return RUNTIME_STATUS_EXCEPTION;
    }
    if (fi->_this_class->status < CLASS_STATUS_CLINITED) {
        class_clinit(fi->_this_class, runtime);
    }
    if (fi->isrefer) {
        push_ref(stack, *((__refer *) (getInstanceFieldPtr(fins, fi))));
    } else {
        // check variable type to determine s64/s32/f64/f32
        s32 data_bytes = fi->datatype_bytes;
        switch (data_bytes) {
            case 4: {
                push_int(stack, *((s32 *) (getInstanceFieldPtr(fins, fi))));
                break;
            }
            case 1: {
                push_int(stack, *((s8 *) (getInstanceFieldPtr(fins, fi))));
                break;
            }
            case 8: {
                push_long(stack, *((s64 *) (getInstanceFieldPtr(fins, fi))));
                break;
            }
            case 2: {
                if (fi->datatype_idx == DATATYPE_JCHAR) {
                    push_int(stack, *((u16 *) (getInstanceFieldPtr(fins, fi))));
                } else {
                    push_int(stack, *((s16 *) (getInstanceFieldPtr(fins, fi))));
                }
                break;
            }
            default: {
                break;
            }
        }
    }
    return RUNTIME_STATUS_NORMAL;
}

static inline int _optimize_inline_setter(JClass *clazz, s32 cmrIdx, Runtime *runtime) {

    RuntimeStack *stack = runtime->stack;
    FieldInfo *fi = class_get_constant_fieldref(clazz, cmrIdx)->fieldInfo;
    if (!fi) {
        ConstantFieldRef *cfr = class_get_constant_fieldref(clazz, cmrIdx);
        fi = find_fieldInfo_by_fieldref(clazz, cfr->item.index, runtime);
        cfr->fieldInfo = fi;
        if (!fi) {
            _nosuchfield_check_exception(utf8_cstr(cfr->name), runtime->stack, runtime);
            return RUNTIME_STATUS_EXCEPTION;
        }
    }

    if (fi->_this_class->status < CLASS_STATUS_CLINITED) {
        class_clinit(fi->_this_class, runtime);
    }
    if (fi->isrefer) {
        __refer v = pop_ref(stack);
        Instance *fins = pop_ref(stack);
        if (!fins) {
            _null_throw_exception(stack, runtime);
            return RUNTIME_STATUS_EXCEPTION;
        }
        *((__refer *) (getInstanceFieldPtr(fins, fi))) = v;
    } else {
        // check variable type to determine s64/s32/f64/f32
        s32 data_bytes = fi->datatype_bytes;
        switch (data_bytes) {
            case 4: {
                s32 v = pop_int(stack);
                Instance *fins = pop_ref(stack);
                if (!fins) {
                    _null_throw_exception(stack, runtime);
                    return RUNTIME_STATUS_EXCEPTION;
                }
                *((s32 *) (getInstanceFieldPtr(fins, fi))) = v;
                break;
            }
            case 1: {
                s32 v = pop_int(stack);
                Instance *fins = pop_ref(stack);
                if (!fins) {
                    _null_throw_exception(stack, runtime);
                    return RUNTIME_STATUS_EXCEPTION;
                }
                *((s8 *) (getInstanceFieldPtr(fins, fi))) = v;
                break;
            }
            case 8: {
                s64 v = pop_long(stack);
                Instance *fins = pop_ref(stack);
                if (!fins) {
                    _null_throw_exception(stack, runtime);
                    return RUNTIME_STATUS_EXCEPTION;
                }
                *((s64 *) (getInstanceFieldPtr(fins, fi))) = v;
                break;
            }
            case 2: {
                s32 v = pop_int(stack);
                Instance *fins = pop_ref(stack);
                if (!fins) {
                    _null_throw_exception(stack, runtime);
                    return RUNTIME_STATUS_EXCEPTION;
                }
                *((u16 *) (getInstanceFieldPtr(fins, fi))) = v;
                break;
            }
            default: {
                break;
            }
        }
    }
    return RUNTIME_STATUS_NORMAL;
}

s32 execute_method_impl(MethodInfo *method, Runtime *pruntime) {


    s32 ret = RUNTIME_STATUS_NORMAL;
    Runtime *runtime = runtime_create_inl(pruntime);

    JClass *clazz = method->_this_class;
    runtime->clazz = clazz;
    runtime->method = method;
    while (clazz->status < CLASS_STATUS_CLINITING) {
        class_clinit(clazz, runtime);
    }
#if _JVM_DEBUG_BYTECODE_DETAIL > 3
    invoke_deepth(pruntime);
    jvm_printf("%s.%s%s { //\n", utf8_cstr(method->_this_class->name),
               utf8_cstr(method->name), utf8_cstr(method->descriptor));
#endif

//    if (utf8_equals_c(method->name, "<clinit>")
//        && utf8_equals_c(clazz->name, "org/mini/g3d/core/gltf2/loader/GLTFJsonModule")
//            ) {
//        s32 debug = 1;
//    }

    RuntimeStack *stack = runtime->stack;

    if (!(method->is_native)) {
        CodeAttribute *ca = method->converted_code;
        if (ca) {


            if (stack->max_size < (stack->sp - stack->store) + ca->max_stack) {
                Utf8String *ustr = utf8_create();
                getRuntimeStack(runtime, ustr);
                jvm_printf("Stack overflow :\n %s\n", utf8_cstr(ustr));
                utf8_destory(ustr);
                exit(1);
            }
            register u8 *ip = ca->code;
            runtime->pc = ip;
            JavaThreadInfo *threadInfo = runtime->threadInfo;
            c8 *err_msg;
            localvar_init(runtime, ca->max_locals, method->para_slots);
            LocalVarItem *localvar = runtime->localvar;
            register StackEntry *sp = runtime->stack->sp;


            if (method->is_sync)_synchronized_lock_method(method, runtime);

            if (JIT_ENABLE && ca->jit.state == JIT_GEN_SUCCESS) {
                //jvm_printf("jit call %s.%s()\n", method->_this_class->name->data, method->name->data);
                ca->jit.func(method, runtime);
                switch (method->return_slots) {
                    case 0: {// V
                        localvar_dispose(runtime);
                        break;
                    }
                    case 1: { // F I R
                        StackEntry entry;
                        peek_entry(stack->sp - method->return_slots, &entry);
                        localvar_dispose(runtime);
                        push_entry(stack, &entry);
                        break;
                    }
                    case 2: {//J D return type , 2slots
                        s64 v = pop_long(stack);
                        localvar_dispose(runtime);
                        push_long(stack, v);
                        break;
                    }
                    default: {
                        break;
                    }
                }
            } else {
                if (JIT_ENABLE && ca->jit.state == JIT_GEN_UNKNOW) {
                    if (ca->jit.interpreted_count++ > JIT_COMPILE_EXEC_COUNT) {
                        spin_lock(&ca->compile_lock);
                        if (ca->jit.state == JIT_GEN_UNKNOW) {//re test
                            //jvm_printf("enter jit %s.%s()\n", utf8_cstr(method->_this_class->name), utf8_cstr(method->name));
                            construct_jit(method, runtime);
                        }
                        spin_unlock(&ca->compile_lock);
                    }
                }


                do {
                    u8 cur_inst = *ip;

                    if (jdwp_enable) {
                        //breakpoint
                        stack->sp = sp;
                        runtime->pc = ip;
                        if (method->breakpoint) {
                            jdwp_check_breakpoint(runtime);
                        }
                        //debug step
                        if (threadInfo->jdwp_step.active) {//单步状态
                            threadInfo->jdwp_step.bytecode_count++;
                            jdwp_check_debug_step(runtime);

                        }
                        sp = stack->sp;
                        check_gc_pause(-1);
                    }


#if _JVM_DEBUG_PROFILE
                    s64 spent = 0;
                    s64 start_at = nanoTime();
#endif

                    /* ==================================opcode start =============================*/
#ifdef __JVM_DEBUG__
                    s64 inst_pc = runtime->pc - ca->code;
#endif
                    switch (cur_inst) {

                        case op_nop: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("nop\n");
#endif
                            ip++;

                            break;
                        }

                        case op_aconst_null: {
                            (sp++)->rvalue = NULL;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("aconst_null: push %d into stack\n", 0);
#endif
                            ip++;

                            break;
                        }

                        case op_iconst_m1: {
                            (sp++)->ivalue = -1;
                            ip++;
                            break;
                        }


                        case op_iconst_0: {
                            (sp++)->ivalue = 0;
                            ip++;
                            break;
                        }


                        case op_iconst_1: {
                            (sp++)->ivalue = 1;
                            ip++;
                            break;
                        }


                        case op_iconst_2: {
                            (sp++)->ivalue = 2;
                            ip++;
                            break;
                        }


                        case op_iconst_3: {
                            (sp++)->ivalue = 3;
                            ip++;
                            break;
                        }


                        case op_iconst_4: {
                            (sp++)->ivalue = 4;
                            ip++;
                            break;
                        }


                        case op_iconst_5: {
                            (sp++)->ivalue = 5;
                            ip++;
                            break;
                        }


                        case op_lconst_0: {
                            (sp++)->lvalue = 0;
                            sp++;
                            ip++;
                            break;
                        }


                        case op_lconst_1: {
                            (sp++)->lvalue = 1;
                            sp++;
                            ip++;
                            break;
                        }


                        case op_fconst_0: {
                            (sp++)->fvalue = 0;
                            ip++;
                            break;
                        }


                        case op_fconst_1: {
                            (sp++)->fvalue = 1;
                            ip++;
                            break;
                        }


                        case op_fconst_2: {
                            (sp++)->fvalue = 2;
                            ip++;
                            break;
                        }


                        case op_dconst_0: {
                            (sp++)->dvalue = 0;
                            sp++;
                            ip++;
                            break;
                        }


                        case op_dconst_1: {
                            (sp++)->dvalue = 1;
                            sp++;
                            ip++;
                            break;
                        }


                        case op_bipush: {

                            (sp++)->ivalue = (s8) ip[1];
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("bipush a byte %d onto the stack \n", (sp - 1)->ivalue);
#endif
                            ip += 2;

                            break;
                        }


                        case op_sipush: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("sipush value %d\n", *((s16 *) (ip + 1)));
#endif
                            (sp++)->ivalue = *((s16 *) (ip + 1));
                            ip += 3;

                            break;
                        }

                        case op_ldc:
                        case op_ldc_w: {
                            s32 index;
                            if (cur_inst == op_ldc) {
                                index = ip[1];
                                ip += 2;
                            } else {
                                index = *((u16 *) (ip + 1));
                                ip += 3;
                            }
                            ConstantItem *item = class_get_constant_item(clazz, index);
                            switch (item->tag) {
                                case CONSTANT_INTEGER:
                                case CONSTANT_FLOAT: {
                                    s32 v = class_get_constant_integer(clazz, index);
                                    (sp++)->ivalue = v;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                    invoke_deepth(runtime);
                                    jvm_printf("ldc: [%x] \n", v);
#endif
                                    break;
                                }
                                case CONSTANT_STRING_REF: {
                                    ConstantUTF8 *cutf = class_get_constant_utf8(clazz, class_get_constant_stringref(clazz, index)->stringIndex);
                                    (sp++)->rvalue = cutf->jstr;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                    invoke_deepth(runtime);
                                    jvm_printf("ldc: [%llx] =\"%s\"\n", (s64) (intptr_t) cutf->jstr, utf8_cstr(cutf->utfstr));
#endif
                                    break;
                                }
                                case CONSTANT_CLASS: {
                                    stack->sp = sp;
                                    JClass *cl = classes_load_get(class_get_constant_classref(clazz, index)->name, runtime);
                                    if (!cl->ins_class) {
                                        cl->ins_class = insOfJavaLangClass_create_get(runtime, cl);
                                    }
                                    sp = stack->sp;
                                    (sp++)->rvalue = cl->ins_class;
                                    break;
                                }
                                default: {
                                    jvm_printf("ldc: something not implemention \n");
                                }
                            }
                            break;
                        }


                        case op_ldc2_w: {

                            s64 v = class_get_constant_long(clazz, *((u16 *) (ip + 1)));//long or double
                            (sp++)->lvalue = v;
                            sp++;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ldc2_w: push a constant(%d) [%llx] onto the stack \n", *((u16 *) (ip + 1)), (sp - 2)->lvalue);
#endif
                            ip += 3;

                            break;
                        }


                        case op_iload:
                        case op_fload: {
                            (sp++)->ivalue = localvar[(u8) ip[1]].ivalue;
                            ip += 2;
                            break;
                        }

                        case op_aload: {
                            (sp++)->rvalue = localvar[(u8) ip[1]].rvalue;
                            ip += 2;
                            break;
                        }


                        case op_lload:
                        case op_dload: {
                            (sp++)->lvalue = localvar[(u8) ip[1]].lvalue;
                            sp++;
                            ip += 2;
                            break;
                        }

                        case op_fload_0:
                        case op_iload_0: {
                            (sp++)->ivalue = localvar[0].ivalue;
                            ip++;
                            break;
                        }

                        case op_fload_1:
                        case op_iload_1: {
                            (sp++)->ivalue = localvar[1].ivalue;
                            ip++;
                            break;
                        }

                        case op_fload_2:
                        case op_iload_2: {
                            (sp++)->ivalue = localvar[2].ivalue;
                            ip++;
                            break;
                        }

                        case op_fload_3:
                        case op_iload_3: {
                            (sp++)->ivalue = localvar[3].ivalue;
                            ip++;
                            break;
                        }

                        case op_dload_0:
                        case op_lload_0: {
                            (sp++)->lvalue = localvar[0].lvalue;
                            (sp++);
                            ip++;
                            break;
                        }

                        case op_dload_1:
                        case op_lload_1: {
                            (sp++)->lvalue = localvar[1].lvalue;
                            (sp++);
                            ip++;
                            break;
                        }

                        case op_dload_2:
                        case op_lload_2: {
                            (sp++)->lvalue = localvar[2].lvalue;
                            (sp++);
                            ip++;
                            break;
                        }

                        case op_dload_3:
                        case op_lload_3: {
                            (sp++)->lvalue = localvar[3].lvalue;
                            (sp++);
                            ip++;
                            break;
                        }

                        case op_aload_0: {
                            (sp++)->rvalue = localvar[0].rvalue;
                            ip++;
                            break;
                        }


                        case op_aload_1: {
                            (sp++)->rvalue = localvar[1].rvalue;
                            ip++;
                            break;
                        }


                        case op_aload_2: {
                            (sp++)->rvalue = localvar[2].rvalue;
                            ip++;
                            break;
                        }


                        case op_aload_3: {
                            (sp++)->rvalue = localvar[3].rvalue;
                            ip++;
                            break;
                        }


                        case op_iaload:
                        case op_faload: {
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                s32 s = *((s32 *) (arr->arr_body) + index);
                                (sp++)->ivalue = s;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("if_aload push arr[%llx].(%d)=%x:%d:%lf into stack\n", (u64) (intptr_t) arr, index,
                                           s, s, *(f32 *) &s);
#endif
                                ip++;
                            }
                            break;
                        }


                        case op_laload:
                        case op_daload: {
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                s64 s = *(((s64 *) arr->arr_body) + index);
                                (sp++)->lvalue = s;
                                (sp++);
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("ld_aload push arr[%llx].(%d)=%llx:%lld:%lf into stack\n", (u64) (intptr_t) arr, index,
                                           s, s, *(f64 *) &s);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_aaload: {
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                __refer s = *(((__refer *) arr->arr_body) + index);
                                (sp++)->rvalue = s;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("aaload push arr[%llx].(%d)=%llx:%lld into stack\n", (u64) (intptr_t) arr, index,
                                           (s64) (intptr_t) s, (s64) (intptr_t) s);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_baload: {
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                s32 s = *(((s8 *) arr->arr_body) + index);
                                (sp++)->ivalue = s;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("iaload push arr[%llx].(%d)=%x:%d:%lf into stack\n", (u64) (intptr_t) arr, index,
                                           s, s, *(f32 *) &s);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_caload: {
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                s32 s = *(((u16 *) arr->arr_body) + index);
                                (sp++)->ivalue = s;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("iaload push arr[%llx].(%d)=%x:%d:%lf into stack\n", (u64) (intptr_t) arr, index,
                                           s, s, *(f32 *) &s);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_saload: {
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                s32 s = *(((s16 *) arr->arr_body) + index);
                                (sp++)->ivalue = s;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("iaload push arr[%llx].(%d)=%x:%d:%lf into stack\n", (u64) (intptr_t) arr, index,
                                           s, s, *(f32 *) &s);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_istore:
                        case op_fstore: {
                            localvar[(u8) ip[1]].ivalue = (--sp)->ivalue;
                            ip += 2;
                            break;
                        }

                        case op_astore: {
                            localvar[(u8) ip[1]].rvalue = (--sp)->rvalue;
                            ip += 2;
                            break;
                        }


                        case op_lstore:
                        case op_dstore: {
                            --sp;
                            --sp;
                            localvar[(u8) ip[1]].lvalue = (sp)->lvalue;
                            ip += 2;

                            break;
                        }

                        case op_fstore_0:
                        case op_istore_0: {
                            localvar[0].ivalue = (--sp)->ivalue;
                            ip++;
                            break;
                        }

                        case op_fstore_1:
                        case op_istore_1: {
                            localvar[1].ivalue = (--sp)->ivalue;
                            ip++;
                            break;
                        }

                        case op_fstore_2:
                        case op_istore_2: {
                            localvar[2].ivalue = (--sp)->ivalue;
                            ip++;
                            break;
                        }

                        case op_fstore_3:
                        case op_istore_3: {
                            localvar[3].ivalue = (--sp)->ivalue;
                            ip++;
                            break;
                        }

                        case op_dstore_0:
                        case op_lstore_0: {
                            --sp;
                            localvar[0].lvalue = (--sp)->lvalue;
                            ip++;
                            break;
                        }

                        case op_dstore_1:
                        case op_lstore_1: {
                            --sp;
                            localvar[1].lvalue = (--sp)->lvalue;
                            ip++;
                            break;
                        }

                        case op_dstore_2:
                        case op_lstore_2: {
                            --sp;
                            localvar[2].lvalue = (--sp)->lvalue;
                            ip++;
                            break;
                        }

                        case op_dstore_3:
                        case op_lstore_3: {
                            --sp;
                            localvar[3].lvalue = (--sp)->lvalue;
                            ip++;
                            break;
                        }

                        case op_astore_0: {
                            localvar[0].rvalue = (--sp)->rvalue;
                            ip++;
                            break;
                        }


                        case op_astore_1: {
                            localvar[1].rvalue = (--sp)->rvalue;
                            ip++;
                            break;
                        }


                        case op_astore_2: {
                            localvar[2].rvalue = (--sp)->rvalue;
                            ip++;
                            break;
                        }


                        case op_astore_3: {
                            localvar[3].rvalue = (--sp)->rvalue;
                            ip++;
                            break;
                        }


                        case op_fastore:
                        case op_iastore: {
                            s32 i = (--sp)->ivalue;
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                *(((s32 *) arr->arr_body) + index) = i;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("iastore: save array[%llx].(%d)=%d)\n",
                                           (s64) (intptr_t) arr, index, i);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_dastore:
                        case op_lastore: {
                            --sp;
                            s64 j = (--sp)->lvalue;
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                *(((s64 *) arr->arr_body) + index) = j;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("iastore: save array[%llx].(%d)=%lld)\n",
                                           (s64) (intptr_t) arr, index, j);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_aastore: {
                            __refer r = (--sp)->rvalue;
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                *(((__refer *) arr->arr_body) + index) = r;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("iastore: save array[%llx].(%d)=%llx)\n",
                                           (s64) (intptr_t) arr, index, (s64) (intptr_t) r);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_bastore: {
                            s32 i = (--sp)->ivalue;
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                *(((s8 *) arr->arr_body) + index) = (s8) i;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("iastore: save array[%llx].(%d)=%d)\n",
                                           (s64) (intptr_t) arr, index, i);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_castore: {
                            s32 i = (--sp)->ivalue;
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                *(((u16 *) arr->arr_body) + index) = (u16) i;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("iastore: save array[%llx].(%d)=%d)\n",
                                           (s64) (intptr_t) arr, index, i);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_sastore: {
                            s32 i = (--sp)->ivalue;
                            s32 index = (--sp)->ivalue;
                            Instance *arr = (--sp)->rvalue;
                            if (!arr) {
                                goto label_null_throw;
                            } else if (index < 0 || index >= arr->arr_length) {
                                goto label_outofbounds_throw;
                            } else {
                                *(((s16 *) arr->arr_body) + index) = (s16) i;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("iastore: save array[%llx].(%d)=%d)\n",
                                           (s64) (intptr_t) arr, index, i);
#endif

                                ip++;
                            }
                            break;
                        }


                        case op_pop: {
                            --sp;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("pop\n");
#endif
                            ip++;

                            break;
                        }


                        case op_pop2: {
                            --sp;
                            --sp;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("pop2\n");
#endif
                            ip++;

                            break;
                        }


                        case op_dup: {
                            *(sp - 0) = *(sp - 1);
                            sp++;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("dup\n");
#endif
                            ip++;

                            break;
                        }


                        case op_dup_x1: {
                            *(sp - 0) = *(sp - 1);
                            *(sp - 1) = *(sp - 2);
                            *(sp - 2) = *(sp - 0);
                            sp++;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("dup_x1\n");
#endif
                            ip++;

                            break;
                        }


                        case op_dup_x2: {
                            *(sp - 0) = *(sp - 1);
                            *(sp - 1) = *(sp - 2);
                            *(sp - 2) = *(sp - 3);
                            *(sp - 3) = *(sp - 0);
                            sp++;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("dup_x2 \n");
#endif
                            ip++;

                            break;
                        }


                        case op_dup2: {
                            *(sp - 0) = *(sp - 2);
                            *(sp + 1) = *(sp - 1);
                            sp++;
                            sp++;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("dup2\n");
#endif
                            ip++;

                            break;
                        }


                        case op_dup2_x1: {
                            sp++;
                            sp++;
                            *(sp - 1) = *(sp - 3);
                            *(sp - 2) = *(sp - 4);
                            *(sp - 3) = *(sp - 5);
                            *(sp - 5) = *(sp - 2);
                            *(sp - 4) = *(sp - 1);


#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("dup2_x1\n");
#endif
                            ip++;

                            break;
                        }


                        case op_dup2_x2: {
                            sp++;
                            sp++;
                            *(sp - 1) = *(sp - 3);
                            *(sp - 2) = *(sp - 4);
                            *(sp - 3) = *(sp - 5);
                            *(sp - 4) = *(sp - 6);
                            *(sp - 6) = *(sp - 2);
                            *(sp - 5) = *(sp - 1);

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("dup2_x2\n");
#endif
                            ip++;

                            break;
                        }


                        case op_swap: {

                            *(sp - 0) = *(sp - 2);
                            *(sp - 2) = *(sp - 1);
                            *(sp - 1) = *(sp - 0);


#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("swap\n");
#endif
                            ip++;

                            break;
                        }


                        case op_iadd: {

                            --sp;
                            (sp - 1)->ivalue += (sp - 0)->ivalue;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("iadd:  %d\n", (sp - 1)->ivalue);
#endif
                            ip++;

                            break;
                        }


                        case op_ladd: {
                            --sp;
                            --sp;
                            (sp - 2)->lvalue += (sp - 0)->lvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ladd:  %lld\n", (sp - 2)->lvalue);
#endif
                            ip++;

                            break;
                        }


                        case op_fadd: {
                            --sp;
                            (sp - 1)->fvalue += (sp - 0)->fvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("fadd:  %f\n", (sp - 1)->fvalue);
#endif
                            ip++;

                            break;
                        }


                        case op_dadd: {
                            --sp;
                            --sp;
                            (sp - 2)->dvalue += (sp - 0)->dvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("dadd:  %lf\n", (sp - 2)->dvalue);
#endif
                            ip++;

                            break;
                        }


                        case op_isub: {
                            --sp;
                            (sp - 1)->ivalue -= (sp - 0)->ivalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("isub :  %d\n", (sp - 1)->ivalue);
#endif
                            ip++;

                            break;
                        }


                        case op_lsub: {
                            --sp;
                            --sp;
                            (sp - 2)->lvalue -= (sp - 0)->lvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("lsub:  %lld\n", (sp - 2)->lvalue);
#endif
                            ip++;

                            break;
                        }


                        case op_fsub: {
                            --sp;
                            (sp - 1)->fvalue -= (sp - 0)->fvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("fsub:  %f\n", (sp - 1)->fvalue);
#endif
                            ip++;

                            break;
                        }


                        case op_dsub: {
                            --sp;
                            --sp;
                            (sp - 2)->dvalue -= (sp - 0)->dvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("dsub:  %lf\n", (sp - 2)->dvalue);
#endif

                            ip++;

                            break;
                        }


                        case op_imul: {

                            --sp;
                            (sp - 1)->ivalue *= (sp - 0)->ivalue;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("imul:  %d\n", (sp - 1)->ivalue);
#endif
                            ip++;

                            break;
                        }


                        case op_lmul: {
                            --sp;
                            --sp;
                            (sp - 2)->lvalue *= (sp - 0)->lvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("lmul:  %lld\n", (sp - 2)->lvalue);
#endif
                            ip++;

                            break;
                        }


                        case op_fmul: {
                            --sp;
                            (sp - 1)->fvalue *= (sp - 0)->fvalue;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("fmul:  %f\n", (sp - 1)->fvalue);
#endif
                            ip++;

                            break;
                        }


                        case op_dmul: {
                            --sp;
                            --sp;
                            (sp - 2)->dvalue *= (sp - 0)->dvalue;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("dmul:  %lf\n", (sp - 2)->dvalue);
#endif
                            ip++;

                            break;
                        }


                        case op_idiv: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("idiv:  %d\n", (sp - 1)->ivalue);
#endif
                            if (!(sp - 1)->ivalue) {
                                goto label_arrithmetic_throw;
                            } else {
                                --sp;
                                (sp - 1)->ivalue /= (sp - 0)->ivalue;
                                ip++;
                            }

                            break;
                        }


                        case op_ldiv: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ldiv:  %lld\n", (sp - 2)->lvalue);
#endif
                            if (!(sp - 2)->lvalue) {
                                goto label_arrithmetic_throw;
                            } else {
                                --sp;
                                --sp;
                                (sp - 2)->lvalue /= (sp - 0)->lvalue;
                                ip++;
                            }

                            break;
                        }


                        case op_fdiv: {
                            --sp;
                            (sp - 1)->fvalue /= (sp - 0)->fvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("fdiv:  %f\n", (sp - 1)->fvalue);
#endif
                            ip++;

                            break;
                        }

                        case op_ddiv: {
                            --sp;
                            --sp;
                            (sp - 2)->dvalue /= (sp - 0)->dvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ddiv:  %lf\n", (sp - 2)->dvalue);
#endif
                            ip++;

                            break;
                        }


                        case op_irem: {
                            --sp;
                            (sp - 1)->ivalue %= (sp - 0)->ivalue;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("irem:  %d\n", (sp - 1)->ivalue);
#endif
                            ip++;

                            break;
                        }


                        case op_lrem: {
                            --sp;
                            --sp;
                            (sp - 2)->lvalue %= (sp - 0)->lvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("lrem:  %lld\n", (sp - 2)->lvalue);
#endif
                            ip++;

                            break;
                        }


                        case op_frem: {
                            --sp;
                            f32 value1 = (sp - 0)->fvalue;
                            f32 value2 = (sp - 1)->fvalue;
                            f32 v = value2 - ((int) (value2 / value1) * value1);
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("frem:  %f\n", (sp - 1)->fvalue);
#endif
                            (sp - 1)->fvalue = v;
                            ip++;

                            break;
                        }


                        case op_drem: {
                            --sp;
                            --sp;
                            f64 value1 = (sp - 0)->dvalue;
                            f64 value2 = (sp - 2)->dvalue;
                            f64 v = value2 - ((s64) (value2 / value1) * value1);;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("drem:  %lf\n", (sp - 2)->dvalue);
#endif
                            (sp - 2)->dvalue = v;
                            ip++;

                            break;
                        }


                        case op_ineg: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ineg:  %d\n", (sp - 1)->ivalue);
#endif
                            (sp - 1)->ivalue = -(sp - 1)->ivalue;
                            ip++;

                            break;
                        }


                        case op_lneg: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("lneg:  %lld\n", (sp - 2)->lvalue);
#endif
                            (sp - 2)->lvalue = -(sp - 2)->lvalue;
                            ip++;

                            break;
                        }


                        case op_fneg: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("fneg:  %f\n", (sp - 1)->fvalue);
#endif
                            (sp - 1)->fvalue = -(sp - 1)->fvalue;
                            ip++;

                            break;
                        }


                        case op_dneg: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("dneg:  %lf\n", (sp - 2)->dvalue);
#endif
                            (sp - 2)->dvalue = -(sp - 2)->dvalue;
                            ip++;

                            break;
                        }


                        case op_ishl: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ishl: %x << %x  \n", (sp - 2)->ivalue, (sp - 1)->ivalue);
#endif
                            --sp;
                            (sp - 1)->ivalue <<= (sp - 0)->ivalue;
                            ip++;

                            break;
                        }


                        case op_lshl: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("lshl: %llx << %x  \n", (sp - 3)->lvalue, (sp - 1)->ivalue);
#endif
                            --sp;
                            (sp - 2)->lvalue <<= (sp - 0)->ivalue;
                            ip++;

                            break;
                        }


                        case op_ishr: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ishr: %x >> %x  \n", (sp - 2)->ivalue, (sp - 1)->ivalue);
#endif
                            --sp;
                            (sp - 1)->ivalue >>= (sp - 0)->ivalue;
                            ip++;

                            break;
                        }


                        case op_lshr: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("lshr: %llx >> %x  \n", (sp - 3)->lvalue, (sp - 1)->ivalue);
#endif
                            --sp;
                            (sp - 2)->lvalue >>= (sp - 0)->ivalue;
                            ip++;

                            break;
                        }


                        case op_iushr: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("iushr: %x >>> %x  \n", (sp - 2)->uivalue, (sp - 1)->ivalue);
#endif
                            --sp;
                            (sp - 1)->uivalue >>= (sp - 0)->ivalue;
                            ip++;

                            break;
                        }


                        case op_lushr: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("lushr: %llx >>> %x   \n", (sp - 3)->ulvalue, (sp - 1)->ivalue);
#endif
                            --sp;
                            (sp - 2)->ulvalue >>= (sp - 0)->ivalue;
                            ip++;

                            break;
                        }


                        case op_iand: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("iand: %x & %x  \n", (sp - 2)->ivalue, (sp - 1)->ivalue);
#endif
                            --sp;
                            (sp - 1)->ivalue &= (sp - 0)->ivalue;
                            ip++;

                            break;
                        }


                        case op_land: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("land: %llx  &  %llx  \n", (sp - 4)->lvalue, (sp - 2)->lvalue);
#endif
                            --sp;
                            --sp;
                            (sp - 2)->lvalue &= (sp - 0)->lvalue;
                            ip++;

                            break;
                        }


                        case op_ior: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ior: %x & %x  \n", (sp - 2)->ivalue, (sp - 1)->ivalue);
#endif
                            --sp;
                            (sp - 1)->ivalue |= (sp - 0)->ivalue;
                            ip++;

                            break;
                        }


                        case op_lor: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("lor: %llx  |  %llx   \n", (sp - 4)->lvalue, (sp - 2)->lvalue);
#endif
                            --sp;
                            --sp;
                            (sp - 2)->lvalue |= (sp - 0)->lvalue;
                            ip++;

                            break;
                        }


                        case op_ixor: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ixor: %x ^ %x  \n", (sp - 2)->ivalue, (sp - 1)->ivalue);
#endif
                            --sp;
                            (sp - 1)->ivalue ^= (sp - 0)->ivalue;
                            ip++;

                            break;
                        }


                        case op_lxor: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("lxor: %llx  ^  %llx  \n", (sp - 4)->lvalue, (sp - 2)->lvalue);
#endif
                            --sp;
                            --sp;
                            (sp - 2)->lvalue ^= (sp - 0)->lvalue;
                            ip++;

                            break;
                        }


                        case op_iinc: {
                            localvar[(u8) ip[1]].ivalue += (s8) ip[2];
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("iinc: localvar(%d) = %d , inc %d\n", (u8) ip[1], runtime->localvar[(u8) ip[1]].ivalue, (s8) ip[2]);
#endif

                            break;
                        }


                        case op_i2l: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("i2l:  %d\n", (sp - 1)->ivalue);
#endif
                            ++sp;
                            (sp - 2)->lvalue = (s64) (sp - 2)->ivalue;
                            ip++;

                            break;
                        }


                        case op_i2f: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("i2f:  %d\n", (sp - 1)->ivalue);
#endif
                            (sp - 1)->fvalue = (f32) (sp - 1)->ivalue;
                            ip++;

                            break;
                        }


                        case op_i2d: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("i2d:  %d\n", (sp - 1)->ivalue);
#endif
                            ++sp;
                            (sp - 2)->dvalue = (f64) (sp - 2)->ivalue;
                            ip++;

                            break;
                        }


                        case op_l2i: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("l2i:  %lld\n", (sp - 2)->lvalue);
#endif
                            --sp;
                            (sp - 1)->ivalue = (s32) (sp - 1)->lvalue;
                            ip++;

                            break;
                        }


                        case op_l2f: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("l2f:  %lld\n", (sp - 2)->lvalue);
#endif
                            --sp;
                            (sp - 1)->fvalue = (f32) (sp - 1)->lvalue;
                            ip++;

                            break;
                        }


                        case op_l2d: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("l2d:  %lld\n", (sp - 2)->lvalue);
#endif
                            (sp - 2)->dvalue = (f64) (sp - 2)->lvalue;
                            ip++;

                            break;
                        }


                        case op_f2i: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("f2i: %f\n", (sp - 1)->fvalue);
#endif
                            (sp - 1)->ivalue = (s32) (sp - 1)->fvalue;
                            ip++;

                            break;
                        }


                        case op_f2l: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("f2l: %f\n", (sp - 1)->fvalue);
#endif
                            ++sp;
                            (sp - 2)->lvalue = (s64) (sp - 2)->fvalue;
                            ip++;

                            break;
                        }


                        case op_f2d: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("f2d: %f\n", (sp - 1)->fvalue);
#endif
                            ++sp;
                            (sp - 2)->dvalue = (f64) (sp - 2)->fvalue;
                            ip++;

                            break;
                        }


                        case op_d2i: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("d2i: %lf\n", (sp - 2)->dvalue);
#endif
                            --sp;
                            (sp - 1)->ivalue = (s32) (sp - 1)->dvalue;
                            ip++;

                            break;
                        }


                        case op_d2l: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("d2l: %lf\n", (sp - 2)->dvalue);
#endif
                            (sp - 2)->lvalue = (s64) (sp - 2)->dvalue;
                            ip++;

                            break;
                        }


                        case op_d2f: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("d2f: %lf\n", (sp - 2)->dvalue);
#endif
                            --sp;
                            (sp - 1)->fvalue = (f32) (sp - 1)->dvalue;
                            ip++;

                            break;
                        }


                        case op_i2b: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("i2b:  %d\n", (sp - 1)->ivalue);
#endif
                            (sp - 1)->ivalue = (s8) (sp - 1)->ivalue;
                            ip++;

                            break;
                        }


                        case op_i2c: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("i2c:  %d\n", (sp - 1)->ivalue);
#endif
                            (sp - 1)->ivalue = (u16) (sp - 1)->ivalue;
                            ip++;

                            break;
                        }

                        case op_i2s: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("i2s:  %d\n", (sp - 1)->ivalue);
#endif
                            (sp - 1)->ivalue = (s16) (sp - 1)->ivalue;
                            ip++;

                            break;
                        }


                        case op_lcmp: {
                            --sp;
                            s64 value1 = (--sp)->lvalue;
                            --sp;
                            s64 value2 = (--sp)->lvalue;
                            s32 result = value2 == value1 ? 0 : (value2 > value1 ? 1 : -1);

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("lcmp: %llx cmp %llx = %d\n", value2, value1, result);
#endif
                            (sp++)->ivalue = result;

                            ip++;

                            break;
                        }


                        case op_fcmpl: {
                            f32 value1 = (--sp)->fvalue;
                            f32 value2 = (--sp)->fvalue;
                            if ((sp + 0)->uivalue == 0x7fc00000 || (sp + 1)->uivalue == 0x7fc00000) {
                                (sp++)->ivalue = -1;
                            } else {
                                s32 result = value2 == value1 ? 0 : (value2 > value1 ? 1 : -1);

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("fcmpl: %f < %f = %d\n", value2, value1, result);
#endif
                                (sp++)->ivalue = result;
                            }
                            ip++;

                            break;
                        }


                        case op_fcmpg: {
                            f32 value1 = (--sp)->fvalue;
                            f32 value2 = (--sp)->fvalue;
                            if ((sp + 0)->uivalue == 0x7fc00000 || (sp + 1)->uivalue == 0x7fc00000) {
                                (sp++)->ivalue = 1;
                            } else {
                                s32 result = value2 == value1 ? 0 : (value2 > value1 ? 1 : -1);

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("fcmpg: %f > %f = %d\n", value2, value1, result);
#endif
                                (sp++)->ivalue = result;
                            }
                            ip++;

                            break;
                        }


                        case op_dcmpl: {
                            --sp;
                            f64 value1 = (--sp)->dvalue;
                            --sp;
                            f64 value2 = (--sp)->dvalue;
                            if ((sp + 0)->ulvalue == 0x7ff8000000000000L || (sp + 2)->ulvalue == 0x7ff8000000000000L) {
                                (sp++)->ivalue = -1;
                            } else {
                                s32 result = value2 == value1 ? 0 : (value2 > value1 ? 1 : -1);

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("dcmpl: %lf < %lf = %d\n", value2, value1, result);
#endif
                                (sp++)->ivalue = result;
                            }
                            ip++;

                            break;
                        }


                        case op_dcmpg: {
                            --sp;
                            f64 value1 = (--sp)->dvalue;
                            --sp;
                            f64 value2 = (--sp)->dvalue;
                            if ((sp + 0)->ulvalue == 0x7ff8000000000000L || (sp + 2)->ulvalue == 0x7ff8000000000000L) {
                                (sp++)->ivalue = 1;
                            } else {
                                s32 result = value2 == value1 ? 0 : (value2 > value1 ? 1 : -1);

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("dcmpg: %lf > %lf = %d\n", value2, value1, result);
#endif
                                (sp++)->ivalue = result;
                            }
                            ip++;

                            break;
                        }


                        case op_ifeq: {
                            s32 val = (--sp)->ivalue;
                            if (val == 0) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ifeq: %d != 0  then jump \n", val);
#endif


                            break;
                        }


                        case op_ifne: {
                            s32 val = (--sp)->ivalue;
                            if (val != 0) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ifne: %d != 0  then jump\n", val);
#endif


                            break;
                        }


                        case op_iflt: {
                            s32 val = (--sp)->ivalue;
                            if (val < 0) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("iflt: %d < 0  then jump  \n", val);
#endif


                            break;
                        }


                        case op_ifge: {
                            s32 val = (--sp)->ivalue;
                            if (val >= 0) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ifge: %d >= 0  then jump \n", val);
#endif


                            break;
                        }


                        case op_ifgt: {
                            s32 val = (--sp)->ivalue;
                            if (val > 0) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ifgt: %d > 0  then jump \n", val);
#endif


                            break;
                        }


                        case op_ifle: {
                            s32 val = (--sp)->ivalue;
                            if (val <= 0) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ifle: %d <= 0  then jump \n", val);
#endif


                            break;
                        }


                        case op_if_icmpeq: {
                            s32 v2 = (--sp)->ivalue;
                            s32 v1 = (--sp)->ivalue;
                            if (v1 == v2) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("if_icmpeq: %lld == %lld \n", (s64) (intptr_t) v1, (s64) (intptr_t) v2);
#endif

                            break;
                        }


                        case op_if_icmpne: {
                            s32 v2 = (--sp)->ivalue;
                            s32 v1 = (--sp)->ivalue;
                            if (v1 != v2) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("if_icmpne: %lld != %lld \n", (s64) (intptr_t) v1, (s64) (intptr_t) v2);
#endif

                            break;
                        }


                        case op_if_icmplt: {
                            s32 v2 = (--sp)->ivalue;
                            s32 v1 = (--sp)->ivalue;
                            if (v1 < v2) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("if_icmplt: %lld < %lld \n", (s64) (intptr_t) v1, (s64) (intptr_t) v2);
#endif

                            break;
                        }


                        case op_if_icmpge: {
                            s32 v2 = (--sp)->ivalue;
                            s32 v1 = (--sp)->ivalue;
                            if (v1 >= v2) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("if_icmpge: %lld >= %lld \n", (s64) (intptr_t) v1, (s64) (intptr_t) v2);
#endif

                            break;
                        }


                        case op_if_icmpgt: {
                            s32 v2 = (--sp)->ivalue;
                            s32 v1 = (--sp)->ivalue;
                            if (v1 > v2) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("if_icmpgt: %lld > %lld \n", (s64) (intptr_t) v1, (s64) (intptr_t) v2);
#endif

                            break;
                        }


                        case op_if_icmple: {
                            s32 v2 = (--sp)->ivalue;
                            s32 v1 = (--sp)->ivalue;
                            if (v1 <= v2) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("if_icmple: %lld <= %lld \n", (s64) (intptr_t) v1, (s64) (intptr_t) v2);
#endif

                            break;
                        }


                        case op_if_acmpeq: {
                            __refer v2 = (--sp)->rvalue;
                            __refer v1 = (--sp)->rvalue;
                            if (v1 == v2) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("if_acmpeq: %lld == %lld \n", (s64) (intptr_t) v1, (s64) (intptr_t) v2);
#endif

                            break;
                        }


                        case op_if_acmpne: {
                            __refer v2 = (--sp)->rvalue;
                            __refer v1 = (--sp)->rvalue;
                            if (v1 != v2) {
                                s32 offset = *((s16 *) (ip + 1));
                                ip += offset;
                                check_gc_pause(offset);
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("if_acmpne: %lld != %lld \n", (s64) (intptr_t) v1, (s64) (intptr_t) v2);
#endif

                            break;
                        }


                        case op_goto: {

                            s32 offset = *((s16 *) (ip + 1));
                            ip += offset;
                            check_gc_pause(offset);

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("goto: %d\n", offset);
#endif
                            break;
                        }


                        case op_jsr: {
                            s32 offset = *((s16 *) (ip + 1));
                            (sp++)->lvalue = (s64) (intptr_t) (ip + 3);
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("jsr: %d\n", offset);
#endif
                            ip += offset;
                            break;
                        }


                        case op_ret: {
                            __returnaddress addr = (__refer) (intptr_t) localvar[(u8) ip[1]].lvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ret: %x\n", (s64) (intptr_t) addr);
#endif
                            ip = (u8 *) addr;
                            break;
                        }


                        case op_tableswitch: {
                            s32 pos = 0;
                            pos = (s32) (4 - ((((u64) (intptr_t) ip) - (u64) (intptr_t) (ca->code)) % 4));//4 byte对齐


                            s32 default_offset = *((s32 *) (ip + pos));
                            pos += 4;
                            s32 low = *((s32 *) (ip + pos));
                            pos += 4;
                            s32 high = *((s32 *) (ip + pos));
                            pos += 4;

                            int val = (--sp)->ivalue;// pop an int from the stack
                            int offset = 0;
                            if (val < low || val > high) {  // if its less than <low> or greater than <high>,
                                offset = default_offset;              // branch to default
                            } else {                        // otherwise
                                pos += (val - low) * 4;

                                offset = *((s32 *) (ip + pos));     // branch to entry in table
                            }

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("tableswitch: val=%d, offset=%d\n", val, offset);
#endif
                            ip += offset;


                            break;
                        }


                        case op_lookupswitch: {
                            s32 pos = 0;
                            pos = (s32) (4 - ((((u64) (intptr_t) ip) - (u64) (intptr_t) (ca->code)) % 4));//4 byte对齐

                            s32 default_offset = *((s32 *) (ip + pos));
                            pos += 4;
                            s32 n = *((s32 *) (ip + pos));
                            pos += 4;
                            s32 i, key;

                            int val = (--sp)->ivalue;// pop an int from the stack
                            int offset = default_offset;
                            for (i = 0; i < n; i++) {

                                key = *((s32 *) (ip + pos));
                                pos += 4;
                                if (key == val) {
                                    offset = *((s32 *) (ip + pos));
                                    break;
                                } else {
                                    pos += 4;
                                }
                            }

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("tableswitch: val=%d, offset=%d\n", val, offset);
#endif
                            ip += offset;

                            break;
                        }


                        case op_lreturn:
                        case op_dreturn: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5

                            StackEntry entry;
                            peek_entry(stack->sp - 1, &entry);
                            invoke_deepth(runtime);
                            jvm_printf("ld_return=%lld/[%llx]\n", entry_2_long(&entry), entry_2_long(&entry));
#endif
                            --sp;
                            s64 v = (--sp)->lvalue;
                            stack->sp = sp;
                            localvar_dispose(runtime);
                            push_long(stack, v);
                            goto label_exit_while;
                            break;
                        }


                        case op_ireturn:
                        case op_freturn: {
                            s32 v = (--sp)->ivalue;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("if_return=%d/%f\n", v, *(f32 *) &v);
#endif
                            stack->sp = sp;
                            localvar_dispose(runtime);
                            push_int(stack, v);
                            goto label_exit_while;
                            break;
                        }
                        case op_areturn: {
                            __refer v = (--sp)->rvalue;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("a_return=[%llx]\n", (s64) (intptr_t) v);
#endif
                            stack->sp = sp;
                            localvar_dispose(runtime);
                            push_ref(stack, v);
                            goto label_exit_while;
                            break;
                        }


                        case op_return: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("return: \n");
#endif
                            stack->sp = sp;
                            localvar_dispose(runtime);
                            goto label_exit_while;
                            break;
                        }


                        case op_getstatic: {
                            stack->sp = sp;
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            if (!fi) {
                                ConstantFieldRef *cfr = class_get_constant_fieldref(clazz, idx);
                                stack->sp = sp;
                                fi = find_fieldInfo_by_fieldref(clazz, cfr->item.index, runtime);
                                sp = stack->sp;
                                cfr->fieldInfo = fi;
                                if (!fi) {
                                    err_msg = utf8_cstr(cfr->name);
                                    goto label_nosuchfield_throw;
                                }
                            }
                            if (fi->_this_class->status < CLASS_STATUS_CLINITED) {
                                stack->sp = sp;
                                class_clinit(fi->_this_class, runtime);
                                sp = stack->sp;
                            }
                            if (fi->isrefer) {
                                *ip = op_getstatic_ref;
                            } else {
                                // check variable type to determine s64/s32/f64/f32
                                s32 data_bytes = fi->datatype_bytes;
                                switch (data_bytes) {
                                    case 4: {
                                        *ip = op_getstatic_int;
                                        break;
                                    }
                                    case 1: {
                                        *ip = op_getstatic_byte;
                                        break;
                                    }
                                    case 8: {
                                        *ip = op_getstatic_long;
                                        break;
                                    }
                                    case 2: {
                                        if (fi->datatype_idx == DATATYPE_JCHAR) {
                                            *ip = op_getstatic_jchar;
                                        } else {
                                            *ip = op_getstatic_short;
                                        }
                                        break;
                                    }
                                    default: {
                                        break;
                                    }
                                }
                            }
                            break;
                        }


                        case op_putstatic: {

                            u16 idx = *((u16 *) (ip + 1));

                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            if (!fi) {
                                ConstantFieldRef *cfr = class_get_constant_fieldref(clazz, idx);
                                stack->sp = sp;
                                fi = find_fieldInfo_by_fieldref(clazz, cfr->item.index, runtime);
                                sp = stack->sp;

                                cfr->fieldInfo = fi;
                                if (!fi) {
                                    err_msg = utf8_cstr(cfr->name);
                                    goto label_nosuchfield_throw;
                                }
                            }
                            if (fi->_this_class->status < CLASS_STATUS_CLINITED) {
                                stack->sp = sp;
                                class_clinit(fi->_this_class, runtime);
                                sp = stack->sp;
                            }
                            if (fi->isrefer) {//垃圾回收标识
                                *ip = op_putstatic_ref;
                            } else {
                                // check variable type to determain long/s32/f64/f32
                                s32 data_bytes = fi->datatype_bytes;
                                //非引用类型
                                switch (data_bytes) {
                                    case 4: {
                                        *ip = op_putstatic_int;
                                        break;
                                    }
                                    case 1: {
                                        *ip = op_putstatic_byte;
                                        break;
                                    }
                                    case 8: {
                                        *ip = op_putstatic_long;
                                        break;
                                    }
                                    case 2: {
                                        *ip = op_putstatic_short;
                                        break;
                                    }
                                    default: {
                                        break;
                                    }
                                }
                            }
                            break;
                        }


                        case op_getfield: {
                            u16 idx;
                            s32 byte_changed = 0;
                            spin_lock(&sys_classloader->lock);
                            {
                                if (*(ip) == op_getfield) {
                                    idx = *((u16 *) (ip + 1));
                                } else {
                                    byte_changed = 1;
                                }
                            }
                            spin_unlock(&sys_classloader->lock);

                            if (!byte_changed) {
                                FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                                if (!fi) {
                                    ConstantFieldRef *cfr = class_get_constant_fieldref(clazz, idx);
                                    stack->sp = sp;
                                    fi = find_fieldInfo_by_fieldref(clazz, cfr->item.index, runtime);
                                    sp = stack->sp;
                                    cfr->fieldInfo = fi;
                                    if (!fi) {
                                        err_msg = utf8_cstr(cfr->name);
                                        goto label_nosuchfield_throw;
                                    }
                                }
                                if (fi->_this_class->status < CLASS_STATUS_CLINITED) {
                                    stack->sp = sp;
                                    class_clinit(fi->_this_class, runtime);
                                    sp = stack->sp;
                                }
                                spin_lock(&sys_classloader->lock);
                                {
                                    if (fi->isrefer) {
                                        *ip = op_getfield_ref;
                                    } else {
                                        // check variable type to determine s64/s32/f64/f32
                                        s32 data_bytes = fi->datatype_bytes;
                                        switch (data_bytes) {
                                            case 4: {
                                                *ip = op_getfield_int;
                                                break;
                                            }
                                            case 1: {
                                                *ip = op_getfield_byte;
                                                break;
                                            }
                                            case 8: {
                                                *ip = op_getfield_long;
                                                break;
                                            }
                                            case 2: {
                                                if (fi->datatype_idx == DATATYPE_JCHAR) {
                                                    *ip = op_getfield_jchar;
                                                } else {
                                                    *ip = op_getfield_short;
                                                }
                                                break;
                                            }
                                            default: {
                                                break;
                                            }
                                        }
                                    }
                                    *((u16 *) (ip + 1)) = fi->offset_instance;
                                }
                                spin_unlock(&sys_classloader->lock);
                            } else {
                                //jvm_printf("getfield byte code changed by other thread.\n");
                            }
                            break;
                        }


                        case op_putfield: {
                            //there were a multithread error , one enter the ins but changed by another
                            u16 idx;
                            s32 byte_changed = 0;
                            spin_lock(&sys_classloader->lock);
                            {
                                if (*(ip) == op_putfield) {
                                    idx = *((u16 *) (ip + 1));
                                } else {
                                    byte_changed = 1;
                                }
                            }
                            spin_unlock(&sys_classloader->lock);

                            if (!byte_changed) {
                                FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                                if (!fi) {
                                    ConstantFieldRef *cfr = class_get_constant_fieldref(clazz, idx);

                                    stack->sp = sp;
                                    fi = find_fieldInfo_by_fieldref(clazz, cfr->item.index, runtime);
                                    sp = stack->sp;
                                    cfr->fieldInfo = fi;
                                    if (!fi) {
                                        err_msg = utf8_cstr(cfr->name);
                                        goto label_nosuchfield_throw;
                                    }
                                }
                                if (fi->_this_class->status < CLASS_STATUS_CLINITED) {
                                    stack->sp = sp;
                                    class_clinit(fi->_this_class, runtime);
                                    sp = stack->sp;
                                }
                                spin_lock(&sys_classloader->lock);
                                {
                                    if (fi->isrefer) {//垃圾回收标识
                                        *ip = op_putfield_ref;
                                    } else {
                                        s32 data_bytes = fi->datatype_bytes;
                                        //非引用类型
                                        switch (data_bytes) {
                                            case 4: {
                                                *ip = op_putfield_int;
                                                break;
                                            }
                                            case 1: {
                                                *ip = op_putfield_byte;
                                                break;
                                            }
                                            case 8: {
                                                *ip = op_putfield_long;
                                                break;
                                            }
                                            case 2: {
                                                *ip = op_putfield_short;
                                                break;
                                            }
                                            default: {
                                                break;
                                            }
                                        }
                                    }
                                    *((u16 *) (ip + 1)) = fi->offset_instance;
                                }
                                spin_unlock(&sys_classloader->lock);
                            } else {
                                //jvm_printf("putfield byte code changed by other thread.\n");
                            }
                            break;
                        }


                        case op_invokevirtual: {

                            ConstantMethodRef *cmr = class_get_constant_method_ref(clazz, *((u16 *) (ip + 1)));

                            Instance *ins = (sp - 1 - cmr->para_slots)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                MethodInfo *m = (MethodInfo *) pairlist_get(cmr->virtual_methods, ins->mb.clazz);
                                if (!m) {
                                    stack->sp = sp;
                                    m = find_instance_methodInfo_by_name(ins, cmr->name, cmr->descriptor, runtime);
                                    sp = stack->sp;
                                    pairlist_put(cmr->virtual_methods, ins->mb.clazz, m);//放入缓存，以便下次直接调用
                                }

                                if (!m) {
                                    err_msg = utf8_cstr(cmr->name);
                                    goto label_nosuchmethod_throw;
                                } else {
                                    s32 match = 0;
                                    if (m->is_getter) {//optimize getter eg:  int getSize(){return size;}
                                        u8 *mc = m->converted_code->bytecode_for_jit;//must use original bytecode
                                        match = 1;
                                        //do getter here
                                        u16 idx = *((u16 *) (mc + 2));//field desc index
                                        JClass *other = m->_this_class;
                                        stack->sp = sp;
                                        ret = _optimize_inline_getter(other, idx, runtime);
                                        sp = stack->sp;
                                        if (ret) {
                                            goto label_exception_handle;
                                        }
                                        //jvm_printf("methodcall getter %s.%s  %d  in    %s.%s\n", utf8_cstr(m->_this_class->name), utf8_cstr(m->name), m->_this_class->status, utf8_cstr(clazz->name), utf8_cstr(method->name));
                                        ip += 3;
                                    } else if (m->is_setter) {//optimize setter eg: void setSize(int size){this.size=size;}
                                        u8 *mc = m->converted_code->bytecode_for_jit;//must use original bytecode
                                        match = 1;
                                        //do setter here
                                        u16 idx = *((u16 *) (mc + 3));//field desc index
                                        JClass *other = m->_this_class;
                                        stack->sp = sp;
                                        ret = _optimize_inline_setter(other, idx, runtime);
                                        sp = stack->sp;
                                        if (ret) {
                                            goto label_exception_handle;
                                        }
                                        //jvm_printf("methodcall setter %s.%s  %d  in    %s.%s\n", utf8_cstr(m->_this_class->name), utf8_cstr(m->name), m->_this_class->status, utf8_cstr(clazz->name), utf8_cstr(method->name));
                                        ip += 3;
                                    }
                                    if (!match) {
                                        *ip = op_invokevirtual_fast;
                                    }
                                }
                            }

                            break;
                        }


                        case op_invokespecial: {
                            ConstantMethodRef *cmr = class_get_constant_method_ref(clazz, *((u16 *) (ip + 1)));
                            MethodInfo *m = cmr->methodInfo;

                            if (!m) {
                                err_msg = utf8_cstr(cmr->name);
                                goto label_nosuchmethod_throw;
                            } else {
                                *ip = op_invokespecial_fast;
                                _optimize_empty_method_call(m, ca, ip);//if method is empty ,bytecode would replaced 'nop' and 'pop' para
                            }
                            break;
                        }


                        case op_invokestatic: {
                            ConstantMethodRef *cmr = class_get_constant_method_ref(clazz, *((u16 *) (ip + 1)));
                            MethodInfo *m = cmr->methodInfo;

                            if (!m) {
                                err_msg = utf8_cstr(cmr->name);
                                goto label_nosuchmethod_throw;
                            } else {
                                *ip = op_invokestatic_fast;
                                _optimize_empty_method_call(m, ca, ip);
                            }
                            break;
                        }


                        case op_invokeinterface: {

                            //s32 paraCount = (u8) ip[3];
                            ConstantMethodRef *cmr = class_get_constant_method_ref(clazz, *((u16 *) (ip + 1)));
                            Instance *ins = (sp - 1 - cmr->para_slots)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                MethodInfo *m = (MethodInfo *) pairlist_get(cmr->virtual_methods, ins->mb.clazz);
                                if (!m) {
                                    stack->sp = sp;
                                    m = find_instance_methodInfo_by_name(ins, cmr->name, cmr->descriptor, runtime);
                                    sp = stack->sp;
                                    pairlist_put(cmr->virtual_methods, ins->mb.clazz, m);//放入缓存，以便下次直接调用
                                }
                                if (!m) {
                                    err_msg = utf8_cstr(cmr->name);
                                    goto label_nosuchmethod_throw;
                                } else {
                                    *ip = op_invokeinterface_fast;
                                }
                            }
                            break;
                        }


                        case op_invokedynamic: {
                            //get bootMethod struct
                            ConstantInvokeDynamic *cid = class_get_invoke_dynamic(clazz, *((u16 *) (ip + 1)));
                            BootstrapMethod *bootMethod = &clazz->bootstrapMethodAttr->bootstrap_methods[cid->bootstrap_method_attr_index];//Boot

                            if (bootMethod->make == NULL) {
                                stack->sp = sp;
                                ret = invokedynamic_prepare(runtime, bootMethod, cid);
                                sp = stack->sp;
                                if (ret) {
                                    goto label_exception_handle;
                                }
                            }
                            MethodInfo *m = bootMethod->make;

                            if (!m) {
                                err_msg = "Lambda generated method";
                                goto label_nosuchmethod_throw;
                            } else {
                                *ip = op_invokedynamic_fast;
                            }
                            break;
                        }


                        case op_new: {
                            s32 i = *((u16 *) (ip + 1));

                            stack->sp = sp;
                            JClass *other = getClassByConstantClassRef(clazz, i, runtime);
                            Instance *ins = NULL;
                            if (other) {
                                ins = instance_create(runtime, other);
                            }
                            sp = stack->sp;

                            (sp++)->rvalue = ins;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("new %s [%llx]\n", utf8_cstr(other->name), (s64) (intptr_t) ins);
#endif
                            ip += 3;
                            break;
                        }


                        case op_newarray: {
                            s32 typeIdx = ip[1];

                            s32 count = (--sp)->ivalue;

                            stack->sp = sp;
                            Instance *arr = jarray_create_by_type_index(runtime, count, typeIdx);
                            sp = stack->sp;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("(a)newarray  [%llx] type:%c , count:%d  \n", (s64) (intptr_t) arr, getDataTypeTag(typeIdx), count);
#endif
                            (sp++)->rvalue = arr;
                            ip += 2;
                            break;
                        }


                        case op_anewarray: {
                            u16 idx = *((u16 *) (ip + 1));

                            s32 count = (--sp)->ivalue;
                            JClass *arr_class = pairlist_get(clazz->arr_class_type, (__refer) (intptr_t) idx);

                            stack->sp = sp;
                            Instance *arr = NULL;
                            if (!arr_class) {//cache to speed
                                arr_class = array_class_get_by_name(runtime, class_get_utf8_string(clazz, idx));
                                pairlist_put(clazz->arr_class_type, (__refer) (intptr_t) idx, arr_class);
                            }
                            arr = jarray_create_by_class(runtime, count, arr_class);
                            sp = stack->sp;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("(a)newarray  [%llx] type:%d , count:%d  \n", (s64) (intptr_t) arr, arr_class->arr_class_type, count);
#endif
                            (sp++)->rvalue = arr;
                            ip += 3;
                            break;
                        }


                        case op_arraylength: {
                            Instance *arr = (--sp)->rvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("arraylength  [%llx].arr_body[%llx] len:%d  \n",
                                       (s64) (intptr_t) arr, (s64) (intptr_t) arr->arr_body, arr->arr_length);
#endif
                            if (!arr) {
                                goto label_null_throw;
                            } else {
                                (sp++)->ivalue = arr->arr_length;
                                ip++;
                            }
                            break;
                        }


                        case op_athrow: {

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            Instance *ins = (Instance *) pop_ref(stack);
                            push_ref(stack, (__refer) ins);
                            invoke_deepth(runtime);
                            jvm_printf("athrow  [%llx].exception throws  \n", (s64) (intptr_t) ins);
#endif
                            goto label_exception_handle;
                            break;
                        }


                        case op_checkcast: {
                            s32 typeIdx = *((u16 *) (ip + 1));

                            Instance *ins = (--sp)->rvalue;

                            stack->sp = sp;
                            if (!checkcast(runtime, ins, typeIdx)) {
                                goto label_checkcast_throw;
                            } else {
                                sp = stack->sp;
                                (sp++)->rvalue = ins;
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("checkcast  %s instancof %s  \n", utf8_cstr(ins->mb.clazz->name), utf8_cstr(class_get_constant_classref(clazz, typeIdx)->name));
#endif

                            break;
                        }


                        case op_instanceof: {
                            Instance *ins = (--sp)->rvalue;
                            s32 typeIdx = *((u16 *) (ip + 1));

                            s32 checkok = 0;
                            if (!ins) {
                            } else if (ins->mb.type & (MEM_TYPE_INS | MEM_TYPE_ARR)) {
                                stack->sp = sp;
                                if (instance_of(ins, getClassByConstantClassRef(clazz, typeIdx, runtime))) {
                                    checkok = 1;
                                }
                                sp = stack->sp;
                            }
                            (sp++)->ivalue = checkok;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("instanceof  [%llx] instancof %s  \n", (s64) (intptr_t) ins, utf8_cstr(class_get_constant_classref(clazz, typeIdx)->name));
#endif
                            ip += 3;
                            break;
                        }


                        case op_monitorenter: {
                            Instance *ins = (--sp)->rvalue;
                            stack->sp = sp;
                            jthread_lock(&ins->mb, runtime);
                            sp = stack->sp;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("monitorenter  [%llx] %s  \n", (s64) (intptr_t) ins, ins ? utf8_cstr(ins->mb.clazz->name) : "null");
#endif
                            ip++;
                            break;
                        }


                        case op_monitorexit: {
                            Instance *ins = (--sp)->rvalue;
                            stack->sp = sp;
                            jthread_unlock(&ins->mb, runtime);
                            sp = stack->sp;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("monitorexit  [%llx] %s  \n", (s64) (intptr_t) ins, ins ? utf8_cstr(ins->mb.clazz->name) : "null");
#endif
                            ip++;
                            break;
                        }


                        case op_wide: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5

                            invoke_deepth(runtime);
                            jvm_printf("wide  \n");
#endif
                            ip++;
                            cur_inst = *ip;
                            switch (cur_inst) {
                                case op_iload:
                                case op_fload: {
                                    (sp++)->ivalue = localvar[*((u16 *) (ip + 1))].ivalue;
                                    ip += 3;
                                    break;
                                }
                                case op_aload: {
                                    (sp++)->rvalue = localvar[*((u16 *) (ip + 1))].rvalue;
                                    ip += 3;
                                    break;
                                }
                                case op_lload:
                                case op_dload: {
                                    (sp++)->lvalue = localvar[*((u16 *) (ip + 1))].lvalue;
                                    sp++;
                                    ip += 3;
                                    break;
                                }
                                case op_istore:
                                case op_fstore: {
                                    localvar[*((u16 *) (ip + 1))].ivalue = (--sp)->ivalue;
                                    ip += 3;
                                    break;
                                }
                                case op_astore: {
                                    localvar[*((u16 *) (ip + 1))].rvalue = (--sp)->rvalue;
                                    ip += 3;
                                    break;
                                }
                                case op_lstore:
                                case op_dstore: {
                                    --sp;
                                    localvar[*((u16 *) (ip + 1))].lvalue = (--sp)->lvalue;
                                    ip += 3;
                                    break;
                                }
                                case op_ret: {
                                    __refer addr = (__refer) (intptr_t) localvar[*((u16 *) (ip + 1))].lvalue;

#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                    invoke_deepth(runtime);
                                    jvm_printf("wide ret: %x\n", (s64) (intptr_t) addr);
#endif
                                    ip = (u8 *) addr;
                                    break;
                                }
                                case op_iinc    : {

                                    localvar[*((u16 *) (ip + 1))].ivalue += *((s16 *) (ip + 3));
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                    invoke_deepth(runtime);
                                    jvm_printf("wide iinc: localvar(%d) = %d , inc %d\n", *((u16 *) (ip + 1)), runtime->localvar[*((u16 *) (ip + 1))].ivalue, *((u16 *) (ip + 3)));
#endif
                                    ip += 5;
                                    break;
                                }
                                default:
                                    _op_notsupport(ip, runtime);
                            }
                            break;
                        }


                        case op_multianewarray: {
                            //data type index

                            Utf8String *desc = class_get_utf8_string(clazz, *((u16 *) (ip + 1)));
                            //array dim
                            s32 count = (u8) ip[3];
#ifdef __JVM_OS_VS__
                            s32 dim[32];
#else
                            s32 dim[count];
#endif
                            int i;
                            for (i = 0; i < count; i++)
                                dim[i] = (--sp)->ivalue;

                            stack->sp = sp;
                            Instance *arr = jarray_multi_create(runtime, dim, count, desc, 0);
                            sp = stack->sp;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("multianewarray  [%llx] type:%s , count:%d  \n", (s64) (intptr_t) arr,
                                       utf8_cstr(desc), count);
#endif
                            (sp++)->rvalue = arr;
                            ip += 4;
                            break;
                        }


                        case op_ifnull: {
                            __refer ref = (--sp)->rvalue;
                            if (!ref) {

                                ip += *((s16 *) (ip + 1));
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ifnonnull: %d/%llx != 0  then jump %d \n", (s32) (intptr_t) ref,
                                       (s64) (intptr_t) ref);
#endif


                            break;
                        }


                        case op_ifnonnull: {
                            __refer ref = (--sp)->rvalue;
                            if (ref) {
                                ip += *((s16 *) (ip + 1));
                            } else {
                                ip += 3;
                            }
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("ifnonnull: %d/%llx != 0  then \n", (s32) (intptr_t) ref, (s64) (intptr_t) ref);
#endif
                            break;
                        }


                        case op_goto_w: {
                            s32 offset = *((s32 *) (ip + 1));
                            ip += offset;
                            check_gc_pause(offset);
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("goto: %d\n", offset);
#endif
                            break;
                        }


                        case op_jsr_w: {

                            s32 offset = *((s32 *) (ip + 1));
                            (sp++)->lvalue = (s64) (__refer) (ip + 3);
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("jsr_w: %d\n", offset);
#endif
                            ip += offset;
                            break;
                        }


                        case op_breakpoint: {
#if _JVM_DEBUG_BYTECODE_DETAIL > 5

                            invoke_deepth(runtime);
                            jvm_printf("breakpoint \n");
#endif
                            break;
                        }


                        case op_getstatic_ref: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);

                            (sp++)->rvalue = *((__refer *) ptr);
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: ref  %d = %s.%s \n", "getstatic", (s64) (intptr_t) getFieldRefer(ptr), utf8_cstr(clazz->name), utf8_cstr(fi->name), getFieldLong(ptr));
#endif
                            break;
                        }

                        case op_getstatic_long: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);
                            (sp++)->lvalue = *((s64 *) ptr);
                            sp++;
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: long  %d = %s.%s \n", "getstatic", getFieldLong(ptr), utf8_cstr(clazz->name), utf8_cstr(fi->name), getFieldLong(ptr));
#endif
                            break;
                        }

                        case op_getstatic_int: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);
                            (sp++)->ivalue = *((s32 *) ptr);
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: int  %d = %s.%s \n", "getstatic", (s32) getFieldInt(ptr), utf8_cstr(clazz->name), utf8_cstr(fi->name), getFieldLong(ptr));
#endif
                            break;
                        }

                        case op_getstatic_short: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);
                            (sp++)->ivalue = *((s16 *) ptr);
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: short  %d = %s.%s \n", "getstatic", (s32) getFieldShort(ptr), utf8_cstr(clazz->name), utf8_cstr(fi->name), getFieldLong(ptr));
#endif
                            break;
                        }

                        case op_getstatic_jchar: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);
                            (sp++)->ivalue = *((u16 *) ptr);
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: char  %d = %s.%s \n", "getstatic", (s32) (u16) getFieldChar(ptr), utf8_cstr(clazz->name), utf8_cstr(fi->name), getFieldLong(ptr));
#endif
                            break;
                        }

                        case op_getstatic_byte: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);
                            (sp++)->ivalue = *((s8 *) ptr);
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: byte  %d = %s.%s \n", "getstatic", (s32) getFieldByte(ptr), utf8_cstr(clazz->name), utf8_cstr(fi->name), getFieldLong(ptr));
#endif
                            break;
                        }


                        case op_putstatic_ref: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);
                            *((__refer *) ptr) = (--sp)->rvalue;
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: ref  %s.%s = %llx \n", "putstatic", utf8_cstr(clazz->name), utf8_cstr(fi->name), (s64) (intptr_t) getFieldRefer(ptr));
#endif
                            break;
                        }


                        case op_putstatic_long: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);
                            --sp;
                            *((s64 *) ptr) = (--sp)->lvalue;
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: long  %s.%s = %lld \n", "putstatic", utf8_cstr(clazz->name), utf8_cstr(fi->name), getFieldLong(ptr));
#endif
                            break;
                        }

                        case op_putstatic_int: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);
                            *((s32 *) ptr) = (--sp)->ivalue;
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: int  %s.%s = %d \n", "putstatic", utf8_cstr(clazz->name), utf8_cstr(fi->name), (s32) getFieldInt(ptr));
#endif
                            break;
                        }

                        case op_putstatic_short: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);
                            *((s16 *) ptr) = (s16) (--sp)->ivalue;
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: short  %s.%s = %d \n", "putstatic", utf8_cstr(clazz->name), utf8_cstr(fi->name), (s32) getFieldShort(ptr));
#endif
                            break;
                        }

                        case op_putstatic_byte: {
                            u16 idx = *((u16 *) (ip + 1));
                            FieldInfo *fi = class_get_constant_fieldref(clazz, idx)->fieldInfo;
                            c8 *ptr = getStaticFieldPtr(fi);
                            *((s8 *) ptr) = (s8) (--sp)->ivalue;
                            ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                            invoke_deepth(runtime);
                            jvm_printf("%s: byte  %s.%s = %d \n", "putstatic", utf8_cstr(clazz->name), utf8_cstr(fi->name), (s32) getFieldByte(ptr));
#endif
                            break;
                        }


                        case op_getfield_ref: {
                            u16 offset = *((u16 *) (ip + 1));
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                c8 *ptr = &(ins->obj_fields[offset]);

                                (sp++)->rvalue = *((__refer *) ptr);
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: ref %llx = %s. \n", "getfield", getFieldRefer(ptr), utf8_cstr(clazz->name));
#endif
                            }
                            break;
                        }


                        case op_getfield_long: {
                            u16 offset = *((u16 *) (ip + 1));
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                c8 *ptr = &(ins->obj_fields[offset]);

                                (sp++)->lvalue = *((s64 *) ptr);
                                sp++;
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: long %lld = %s. \n", "getfield", getFieldLong(ptr), utf8_cstr(ins->mb.clazz->name));
#endif
                            }
                            break;
                        }


                        case op_getfield_int: {
                            u16 offset = *((u16 *) (ip + 1));
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                c8 *ptr = &(ins->obj_fields[offset]);

                                (sp++)->ivalue = *((s32 *) ptr);
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: int %d = %s  \n", "getfield", (s32) getFieldInt(ptr), utf8_cstr(clazz->name));
#endif
                            }
                            break;
                        }


                        case op_getfield_short: {
                            u16 offset = *((u16 *) (ip + 1));
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                c8 *ptr = &(ins->obj_fields[offset]);

                                (sp++)->ivalue = *((s16 *) ptr);
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: short %d = %s \n", "getfield", (s32) getFieldShort(ptr), utf8_cstr(clazz->name));
#endif
                            }
                            break;
                        }


                        case op_getfield_jchar: {
                            u16 offset = *((u16 *) (ip + 1));
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                c8 *ptr = &(ins->obj_fields[offset]);

                                (sp++)->ivalue = *((u16 *) ptr);
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: char %d = %s \n", "getfield", (s32) (u16) getFieldChar(ptr), utf8_cstr(clazz->name));
#endif
                            }
                            break;
                        }


                        case op_getfield_byte: {
                            u16 offset = *((u16 *) (ip + 1));
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                c8 *ptr = &(ins->obj_fields[offset]);

                                (sp++)->ivalue = *((s8 *) ptr);
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: byte %d = %s \n", "getfield", (s32) getFieldByte(ptr), utf8_cstr(clazz->name));
#endif
                            }
                            break;
                        }


                        case op_putfield_ref: {
                            u16 offset = *((u16 *) (ip + 1));
                            __refer v = (--sp)->rvalue;
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                // check variable type to determain long/s32/f64/f32
                                c8 *ptr = &(ins->obj_fields[offset]);
                                *((__refer *) ptr) = v;
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: ref %s\n", "putfield", utf8_cstr(clazz->name));
#endif
                            }
                            break;
                        }


                        case op_putfield_long: {
                            u16 offset = *((u16 *) (ip + 1));
                            --sp;
                            s64 v = (--sp)->lvalue;
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                c8 *ptr = &(ins->obj_fields[offset]);
                                *((s64 *) ptr) = v;
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: long %s\n", "putfield", utf8_cstr(clazz->name));
#endif
                            }
                            break;
                        }


                        case op_putfield_int: {
                            u16 offset = *((u16 *) (ip + 1));
                            s32 v = (--sp)->ivalue;
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                c8 *ptr = &(ins->obj_fields[offset]);
                                *((s32 *) ptr) = v;
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: int %s.= %d\n", "putfield", utf8_cstr(clazz->name), v);
#endif
                            }
                            break;
                        }


                        case op_putfield_short: {
                            u16 offset = *((u16 *) (ip + 1));
                            s32 v = (--sp)->ivalue;
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                c8 *ptr = &(ins->obj_fields[offset]);
                                *((s16 *) ptr) = (s16) v;
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: short %s. = %d\n", "putfield", utf8_cstr(clazz->name), v);
#endif
                            }
                            break;
                        }


                        case op_putfield_byte: {
                            u16 offset = *((u16 *) (ip + 1));
                            s32 v = (--sp)->ivalue;
                            Instance *ins = (--sp)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                c8 *ptr = &(ins->obj_fields[offset]);
                                *((s8 *) ptr) = (s8) v;
                                ip += 3;
#if _JVM_DEBUG_BYTECODE_DETAIL > 5
                                invoke_deepth(runtime);
                                jvm_printf("%s: byte %s. = %d\n", "putfield", utf8_cstr(clazz->name), v);
#endif
                            }
                            break;
                        }


                        case op_invokevirtual_fast: {

                            ConstantMethodRef *cmr = class_get_constant_method_ref(clazz, *((u16 *) (ip + 1)));
                            Instance *ins = (sp - 1 - cmr->para_slots)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                MethodInfo *m = (MethodInfo *) pairlist_get(cmr->virtual_methods, ins->mb.clazz);
#if _JVM_DEBUG_PROFILE
                                spent = nanoTime() - start_at;
#endif
                                if (!m) {
                                    *ip = op_invokevirtual;
                                } else {
                                    stack->sp = sp;
                                    ret = execute_method_impl(m, runtime);
                                    sp = stack->sp;
                                    if (ret) {
                                        goto label_exception_handle;
                                    }
                                    ip += 3;
                                }
                            }

                            break;
                        }


                        case op_invokespecial_fast: {
                            ConstantMethodRef *cmr = class_get_constant_method_ref(clazz, *((u16 *) (ip + 1)));
                            MethodInfo *m = cmr->methodInfo;
#if _JVM_DEBUG_PROFILE
                            spent = nanoTime() - start_at;
#endif
                            stack->sp = sp;
                            ret = execute_method_impl(m, runtime);
                            sp = stack->sp;
                            if (ret) {
                                goto label_exception_handle;
                            }
                            ip += 3;
                            break;
                        }


                        case op_invokestatic_fast: {
                            ConstantMethodRef *cmr = class_get_constant_method_ref(clazz, *((u16 *) (ip + 1)));
                            MethodInfo *m = cmr->methodInfo;
#if _JVM_DEBUG_PROFILE
                            spent = nanoTime() - start_at;
#endif
                            stack->sp = sp;
                            ret = execute_method_impl(m, runtime);
                            sp = stack->sp;
                            if (ret) {
                                goto label_exception_handle;
                            }
                            ip += 3;
                            break;
                        }


                        case op_invokeinterface_fast: {
                            //此cmr所描述的方法，对于不同的实例，有不同的method
                            ConstantMethodRef *cmr = class_get_constant_method_ref(clazz, *((u16 *) (ip + 1)));

                            Instance *ins = (sp - 1 - cmr->para_slots)->rvalue;
                            if (!ins) {
                                goto label_null_throw;
                            } else {
                                MethodInfo *m = (MethodInfo *) pairlist_get(cmr->virtual_methods, ins->mb.clazz);
#if _JVM_DEBUG_PROFILE
                                spent = nanoTime() - start_at;
#endif
                                if (!m) {
                                    *ip = op_invokeinterface;
                                } else {
                                    stack->sp = sp;
                                    ret = execute_method_impl(m, runtime);
                                    sp = stack->sp;
                                    if (ret) {
                                        goto label_exception_handle;
                                    }
                                    ip += 5;
                                }
                            }
                            break;
                        }


                        case op_invokedynamic_fast: {
                            //get bootMethod struct
                            ConstantInvokeDynamic *cid = class_get_invoke_dynamic(clazz, *((u16 *) (ip + 1)));
                            BootstrapMethod *bootMethod = &clazz->bootstrapMethodAttr->bootstrap_methods[cid->bootstrap_method_attr_index];//Boot
                            MethodInfo *m = bootMethod->make;

#if _JVM_DEBUG_PROFILE
                            spent = nanoTime() - start_at;
#endif
                            // run make to generate instance of Lambda Class
                            stack->sp = sp;
                            ret = execute_method_impl(m, runtime);
                            sp = stack->sp;
                            if (ret) {
                                goto label_exception_handle;
                            }

                            ip += 5;
                            break;
                        }

                        default:
                            _op_notsupport(ip, runtime);
                    }

                    /* ================================== runtime->pc end =============================*/

#if _JVM_DEBUG_PROFILE
                    //time
                    if (!spent) spent = nanoTime() - start_at;
                    profile_put(cur_inst, spent, 1);
#endif
                    continue;

                    label_outofbounds_throw:
                    {
                        stack->sp = sp;
                        Instance *exception = exception_create(JVM_EXCEPTION_ARRAYINDEXOUTOFBOUNDS, runtime);
                        push_ref(stack, (__refer) exception);
                        goto label_exception_handle;
                    }

                    label_null_throw:
                    {
                        stack->sp = sp;
                        Instance *exception = exception_create(JVM_EXCEPTION_NULLPOINTER, runtime);
                        push_ref(stack, (__refer) exception);
                        goto label_exception_handle;
                    }

                    label_nosuchmethod_throw:
                    {
                        stack->sp = sp;
                        Instance *exception = exception_create_str(JVM_EXCEPTION_NOSUCHMETHOD, runtime, err_msg);
                        push_ref(stack, (__refer) exception);
                        goto label_exception_handle;
                    }

                    label_nosuchfield_throw:
                    {
                        stack->sp = sp;
                        Instance *exception = exception_create_str(JVM_EXCEPTION_NOSUCHFIELD, runtime, err_msg);
                        push_ref(stack, (__refer) exception);
                        goto label_exception_handle;
                    }

                    label_arrithmetic_throw:
                    {
                        stack->sp = sp;
                        Instance *exception = exception_create(JVM_EXCEPTION_ARRITHMETIC, runtime);
                        push_ref(stack, (__refer) exception);
                        goto label_exception_handle;
                    }

                    label_checkcast_throw:
                    {
                        stack->sp = sp;
                        Instance *exception = exception_create(JVM_EXCEPTION_CLASSCAST, runtime);
                        push_ref(stack, (__refer) exception);
                        goto label_exception_handle;
                    }

                    label_exception_handle:
                    // there is exception handle, but not error/interrupt handle
                    runtime->pc = ip;
                    ret = RUNTIME_STATUS_EXCEPTION;
                    if (exception_handle(runtime->stack, runtime)) {
                        ret = RUNTIME_STATUS_NORMAL;
                        ip = runtime->pc;
                        runtime_clear_stacktrack(runtime);
                    } else {
                        arraylist_push_back(runtime->threadInfo->stacktrack, method);
                        arraylist_push_back(runtime->threadInfo->lineNo, (__refer) (intptr_t) getLineNumByIndex(ca, runtime->pc - ca->code));
                        break;
                    }
                    sp = stack->sp;
#if _JVM_DEBUG_PROFILE
                    //time
                    if (!spent) spent = nanoTime() - start_at;
                    profile_put(cur_inst, spent, 1);
#endif
                    continue;

                    label_exit_while:
#if _JVM_DEBUG_PROFILE
                    //time
                    if (!spent) spent = nanoTime() - start_at;
                    profile_put(cur_inst, spent, 1);
#endif
                    break;

                } while (1);//end while
            }
            if (method->is_sync)_synchronized_unlock_method(method, runtime);

        } else {
            jvm_printf("method code attribute is null.");
        }
    } else {//本地方法
        localvar_init(runtime, method->para_slots, method->para_slots);
        //缓存调用本地方法
        if (!method->native_func) { //把本地方法找出来缓存
            java_native_method *native = find_native_method(utf8_cstr(clazz->name), utf8_cstr(method->name),
                                                            utf8_cstr(method->descriptor));
            if (!native) {
                _nosuchmethod_check_exception(utf8_cstr(method->name), stack, runtime);
                ret = RUNTIME_STATUS_EXCEPTION;
            } else {
                method->native_func = native->func_pointer;
            }
        }

        if (method->native_func) {
            if (method->is_sync)_synchronized_lock_method(method, runtime);
            ret = method->native_func(runtime, clazz);
            if (method->is_sync)_synchronized_unlock_method(method, runtime);
            switch (method->return_slots) {
                case 0: {// V
                    localvar_dispose(runtime);
                    break;
                }
                case 1: { // F I R
                    StackEntry entry;
                    peek_entry(stack->sp - method->return_slots, &entry);
                    localvar_dispose(runtime);
                    push_entry(stack, &entry);
                    break;
                }
                case 2: {//J D return type , 2slots
                    s64 v = pop_long(stack);
                    localvar_dispose(runtime);
                    push_long(stack, v);
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }


#if _JVM_DEBUG_BYTECODE_DETAIL > 3
    if (ret != RUNTIME_STATUS_EXCEPTION) {
        if (stack->sp != runtime->localvar + method->return_slots) {
            invoke_deepth(runtime);
            jvm_printf("stack size  %s.%s%s in:%d out:%d  \n", utf8_cstr(clazz->name), utf8_cstr(method->name), utf8_cstr(method->descriptor), (runtime->localvar - runtime->stack->store), stack_size(stack));
            exit(1);
        }
    }
#endif
    runtime_destory_inl(runtime);
    pruntime->son = NULL;  //need for getLastSon()

#if _JVM_DEBUG_BYTECODE_DETAIL > 3
    invoke_deepth(pruntime);
    jvm_printf("} // %s.%s%s\n", utf8_cstr(method->_this_class->name),
               utf8_cstr(method->name), utf8_cstr(method->descriptor));
#endif

    return ret;
}

