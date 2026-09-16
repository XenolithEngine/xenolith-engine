/**
Copyright (c) 2026 Xenolith Team <admin@xenolith.studio>

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
**/

/*
 * Microsoft Source Annotation Language (SAL).
 *
 * SAL annotations are hints consumed only by the MSVC static analyzer; they
 * have no runtime meaning and no ABI, so — like MinGW-w64's sal.h — every macro
 * here expands to nothing. This lets third-party code that includes <sal.h> and
 * decorates declarations with `_In_`, `_Out_`, `_Ret_`, `__in`, etc. compile
 * unchanged. There is deliberately no abi/sal.h counterpart: SAL carries no
 * materialized values.
 */

#ifndef SPRT_WRAPPERS_WINDOWS_SAL_H_
#define SPRT_WRAPPERS_WINDOWS_SAL_H_

// clang-format off
#define _SAL_VERSION 20
#define __SAL_H_VERSION 180000000

#define SAL_VERSION 20
#define __SAL_H_FULLVER 140050727
// clang-format on

/* Analysis-mode switch: always "off" for a normal (non-analysis) compile. */
#ifndef _USE_ATTRIBUTES_FOR_SAL
#define _USE_ATTRIBUTES_FOR_SAL 0
#endif
#ifndef _USE_DECLSPECS_FOR_SAL
#define _USE_DECLSPECS_FOR_SAL 0
#endif

/* ---- Core buffer / parameter annotations (SAL 2.0) ---------------------- */
#define _In_
#define _In_opt_
#define _In_z_
#define _In_opt_z_
#define _In_reads_(s)
#define _In_reads_opt_(s)
#define _In_reads_bytes_(s)
#define _In_reads_bytes_opt_(s)
#define _In_reads_z_(s)
#define _In_reads_opt_z_(s)
#define _In_reads_or_z_(s)
#define _In_reads_or_z_opt_(s)
#define _In_reads_to_ptr_(p)
#define _In_reads_to_ptr_opt_(p)
#define _In_reads_to_ptr_z_(p)
#define _In_reads_to_ptr_opt_z_(p)

#define _Out_
#define _Out_opt_
#define _Out_writes_(s)
#define _Out_writes_opt_(s)
#define _Out_writes_bytes_(s)
#define _Out_writes_bytes_opt_(s)
#define _Out_writes_z_(s)
#define _Out_writes_opt_z_(s)
#define _Out_writes_to_(s, c)
#define _Out_writes_to_opt_(s, c)
#define _Out_writes_all_(s)
#define _Out_writes_all_opt_(s)
#define _Out_writes_bytes_to_(s, c)
#define _Out_writes_bytes_to_opt_(s, c)
#define _Out_writes_bytes_all_(s)
#define _Out_writes_bytes_all_opt_(s)
#define _Out_writes_to_ptr_(p)
#define _Out_writes_to_ptr_opt_(p)
#define _Out_writes_to_ptr_z_(p)
#define _Out_writes_to_ptr_opt_z_(p)

#define _Inout_
#define _Inout_opt_
#define _Inout_z_
#define _Inout_opt_z_
#define _Inout_updates_(s)
#define _Inout_updates_opt_(s)
#define _Inout_updates_z_(s)
#define _Inout_updates_opt_z_(s)
#define _Inout_updates_to_(s, c)
#define _Inout_updates_to_opt_(s, c)
#define _Inout_updates_all_(s)
#define _Inout_updates_all_opt_(s)
#define _Inout_updates_bytes_(s)
#define _Inout_updates_bytes_opt_(s)
#define _Inout_updates_bytes_to_(s, c)
#define _Inout_updates_bytes_to_opt_(s, c)
#define _Inout_updates_bytes_all_(s)
#define _Inout_updates_bytes_all_opt_(s)

/* ---- Output-pointer annotations ----------------------------------------- */
#define _Outptr_
#define _Outptr_opt_
#define _Outptr_result_maybenull_
#define _Outptr_opt_result_maybenull_
#define _Outptr_result_z_
#define _Outptr_opt_result_z_
#define _Outptr_result_maybenull_z_
#define _Outptr_opt_result_maybenull_z_
#define _Outptr_result_nullonfailure_
#define _Outptr_opt_result_nullonfailure_
#define _Outptr_result_buffer_(s)
#define _Outptr_opt_result_buffer_(s)
#define _Outptr_result_buffer_maybenull_(s)
#define _Outptr_opt_result_buffer_maybenull_(s)
#define _Outptr_result_bytebuffer_(s)
#define _Outptr_opt_result_bytebuffer_(s)
#define _Outptr_result_buffer_to_(s, c)
#define _Outptr_opt_result_buffer_to_(s, c)
#define _Outptr_result_bytebuffer_to_(s, c)
#define _Outptr_opt_result_bytebuffer_to_(s, c)
#define _Outref_
#define _Outref_result_maybenull_
#define _Outref_result_buffer_(s)
#define _Outref_result_bytebuffer_(s)
#define _Outref_result_nullonfailure_

#define _COM_Outptr_
#define _COM_Outptr_opt_
#define _COM_Outptr_result_maybenull_
#define _COM_Outptr_opt_result_maybenull_

#define _Result_nullonfailure_
#define _Result_zeroonfailure_

/* ---- Return-value annotations -------------------------------------------- */
#define _Ret_
#define _Ret_z_
#define _Ret_opt_
#define _Ret_opt_z_
#define _Ret_maybenull_
#define _Ret_maybenull_z_
#define _Ret_null_
#define _Ret_notnull_
#define _Ret_valid_
#define _Ret_range_(l, h)
#define _Ret_writes_(s)
#define _Ret_writes_z_(s)
#define _Ret_writes_bytes_(s)
#define _Ret_writes_maybenull_(s)
#define _Ret_writes_bytes_maybenull_(s)
#define _Ret_writes_to_(s, c)
#define _Ret_writes_bytes_to_(s, c)

#define _Check_return_
#define _Must_inspect_result_
#define _Success_(expr)
#define _Return_type_success_(expr)
#define _On_failure_(annos)
#define _Always_(annos)

/* ---- Pointer / value qualifiers ----------------------------------------- */
#define _Const_
#define _Notnull_
#define _Maybenull_
#define _Null_
#define _Notref_
#define _Reserved_
#define _Pre_z_
#define _Post_z_
#define _Pre_valid_
#define _Post_valid_
#define _Pre_notnull_
#define _Pre_maybenull_
#define _Pre_opt_z_
#define _Prepost_z_

/* ---- String-format annotations ------------------------------------------ */
#define _Printf_format_string_
#define _Scanf_format_string_
#define _Scanf_s_format_string_
#define _Printf_format_string_params_(x)
#define _Scanf_format_string_params_(x)
#define _Format_string_impl_(kind, where)
#define _In_z_bytecount_(s)

/* ---- Field / struct annotations ----------------------------------------- */
#define _Field_size_(s)
#define _Field_size_opt_(s)
#define _Field_size_bytes_(s)
#define _Field_size_bytes_opt_(s)
#define _Field_size_full_(s)
#define _Field_size_full_opt_(s)
#define _Field_size_bytes_full_(s)
#define _Field_size_bytes_full_opt_(s)
#define _Field_size_part_(s, c)
#define _Field_size_bytes_part_(s, c)
#define _Field_z_
#define _Field_range_(l, h)
#define _Struct_size_bytes_(s)

/* ---- General / conditional annotation combinators ----------------------- */
#define _When_(expr, annos)
#define _At_(target, annos)
#define _At_buffer_(target, iter, bound, annos)
#define _Group_(annos)
#define _Pre_
#define _Post_
#define _Deref_
#define _Deref_pre_
#define _Deref_post_
#define _Notliteral_
#define _Literal_
#define _Null_terminated_
#define _NullNull_terminated_
#define _Enum_is_bitflag_
#define _In_range_(l, h)
#define _Out_range_(l, h)
#define _Deref_in_range_(l, h)
#define _Deref_out_range_(l, h)
#define _Deref_inout_range_(l, h)
#define _Deref_out_
#define _Deref_out_opt_
#define _Deref_opt_out_
#define _Deref_opt_out_opt_
#define _Pre_equal_to_(expr)
#define _Post_equal_to_(expr)
#define _Unchanged_(e)
#define _Points_to_data_
#define _Interlocked_operand_
#define _Readable_bytes_(s)
#define _Readable_elements_(s)
#define _Writable_bytes_(s)
#define _Writable_elements_(s)
#define _Null_terminated_impl_

/* ---- Function-level / analysis annotations ------------------------------ */
#define _Use_decl_annotations_
#define _Analysis_assume_(expr)
#define _Analysis_noreturn_
#define _Analysis_assume_nullterminated_(x)
#define _Function_class_(c)
#define _Called_from_function_class_(c)
#define _Raises_SEH_exception_
#define _Maybe_raises_SEH_exception_

/* ---- Lock / concurrency annotations ------------------------------------- */
#define _Acquires_lock_(e)
#define _Releases_lock_(e)
#define _Requires_lock_held_(e)
#define _Requires_lock_not_held_(e)
#define _Requires_no_locks_held_
#define _Guarded_by_(e)
#define _Write_guarded_by_(e)
#define _Interlocked_
#define _Post_satisfies_(expr)
#define _Pre_satisfies_(expr)
#define _Satisfies_(expr)

#endif // SPRT_WRAPPERS_WINDOWS_SAL_H_
