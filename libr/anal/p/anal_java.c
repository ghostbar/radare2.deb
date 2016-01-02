/* radare - Apache 2.0 - Copyright 2010-2014 - pancake and
 Adam Pridgen <dso@rice.edu || adam.pridgen@thecoverofnight.com> */

#include <string.h>

#include <r_types.h>
#include <r_lib.h>
#include <r_asm.h>
#include <r_anal.h>
#include <r_anal_ex.h>
#include <r_cons.h>

#include "../../../shlr/java/code.h"
#include "../../../shlr/java/class.h"

#define IFDBG  if(0)
#define IFINT  if(0)

struct r_anal_java_access_t;

typedef struct r_anal_java_access_t {
	char *method;
	ut64 addr;
	ut64 value;
	ut64 op_type;
	struct r_anal_java_access_t *next;
	struct r_anal_java_access_t *previous;
} RAnalJavaAccess;

typedef struct r_anal_java_local_var_t {
	char *name;
	char *type;
	RList *writes;
	RList *reads;
	RList *binops;
} RAnalJavaLocalVar;

typedef struct r_anal_ex_java_lin_sweep {
	RList *cfg_node_addrs;
}RAnalJavaLinearSweep;

ut64 METHOD_START = 0;


static int java_reset_counter (RAnal *anal, ut64 addr);
static int java_new_method (ut64 addr);
static int java_print_all_definitions( RAnal *anal );
static int java_print_class_definitions( RBinJavaObj *obj );
static int java_print_field_definitions( RBinJavaObj *obj );
static int java_print_method_definitions( RBinJavaObj *obj );
static int java_print_import_definitions( RBinJavaObj *obj );
static void java_update_anal_types (RAnal *anal, RBinJavaObj *bin_obj);
static void java_set_function_prototype (RAnal *anal, RAnalFunction *fcn, RBinJavaField *method);

static int java_print_class_access_flags_value( const char * flags );
static int java_print_field_access_flags_value( const char * flags );
static int java_print_method_access_flags_value( const char * flags );


static int java_cmd_ext(RAnal *anal, const char* input);
static int analyze_from_code_buffer ( RAnal *anal, RAnalFunction *fcn, ut64 addr, const ut8 *code_buf, ut64 code_length);
static int analyze_from_code_attr (RAnal *anal, RAnalFunction *fcn, RBinJavaField *method, ut64 loadaddr);
static int analyze_method(RAnal *anal, RAnalFunction *fcn, RAnalState *state);

static int java_op(RAnal *anal, RAnalOp *op, ut64 addr, const ut8 *data, int len);
//static int java_bb(RAnal *anal, RAnalFunction *fcn, ut64 addr, ut8 *buf, ut64 len, int reftype);
//static int java_fn(RAnal *anal, RAnalFunction *fcn, ut64 addr, ut8 *buf, ut64 len, int reftype);

static int java_recursive_descent(RAnal *anal, RAnalState *state, ut64 addr);
static int handle_bb_cf_recursive_descent (RAnal *anal, RAnalState *state);

static int java_linear_sweep(RAnal *anal, RAnalState *state, ut64 addr);
static int handle_bb_cf_linear_sweep (RAnal *anal, RAnalState *state);
static int java_post_anal_linear_sweep(RAnal *anal, RAnalState *state, ut64 addr);
static RBinJavaObj * get_java_bin_obj(RAnal *anal);
static RList * get_java_bin_obj_list(RAnal *anal);

static int java_analyze_fns( RAnal *anal, ut64 start, ut64 end, int reftype, int depth);

//static RAnalOp * java_op_from_buffer(RAnal *anal, RAnalState *state, ut64 addr);
//static RAnalBlock * java_bb_from_buffer(RAnal *anal, RAnalState *state, ut64 addr);
//static RAnalFunction * java_fn_from_buffer(RAnal *anal, RAnalState *state, ut64 addr);

static int check_addr_in_code (RBinJavaField *method, ut64 addr);
static int check_addr_less_end (RBinJavaField *method, ut64 addr);
static int check_addr_less_start (RBinJavaField *method, ut64 addr);

static int java_revisit_bb_anal_recursive_descent(RAnal *anal, RAnalState *state, ut64 addr);

static RBinJavaObj * get_java_bin_obj(RAnal *anal) {
	RBin *b = anal->binb.bin;
	ut8 is_java = (b && b->cur->curplugin && strcmp (b->cur->curplugin->name, "java") == 0) ? 1 : 0;
	return is_java ? b->cur->o->bin_obj : NULL;
}

static RList * get_java_bin_obj_list(RAnal *anal) {
	RBinJavaObj *bin_obj = (RBinJavaObj * )get_java_bin_obj(anal);
	// See libr/bin/p/bin_java.c to see what is happening here.  The original intention
	// was to use a shared global db variable from shlr/java/class.c, but the
	// BIN_OBJS_ADDRS variable kept getting corrupted on Mac, so I (deeso) switched the
	// way the access to the db was taking place by using the bin_obj as a proxy back
	// to the BIN_OBJS_ADDRS which is instantiated in libr/bin/p/bin_java.c
	// not the easiest way to make sausage, but its getting made.
	return  r_bin_java_get_bin_obj_list_thru_obj (bin_obj);
}

static int check_addr_less_end (RBinJavaField *method, ut64 addr) {
	ut64 end = r_bin_java_get_method_code_size (method);
	if (addr < end)
		return R_TRUE;
	return R_FALSE;
}

static int check_addr_in_code (RBinJavaField *method, ut64 addr) {
	return !check_addr_less_start (method, addr) && \
		check_addr_less_end ( method, addr);
}

static int check_addr_less_start (RBinJavaField *method, ut64 addr) {
	ut64 start = r_bin_java_get_method_code_offset (method);
	if (addr < start)
		return R_TRUE;
	return R_FALSE;
}


static int java_new_method (ut64 method_start) {
	METHOD_START = method_start;
	// reset the current bytes consumed counter
	r_java_new_method ();
	return 0;
}

static ut64 java_get_method_start () {
	return METHOD_START;
}

static int java_revisit_bb_anal_recursive_descent(RAnal *anal, RAnalState *state, ut64 addr) {
	RAnalBlock *current_head = state && state->current_bb_head ? state->current_bb_head : NULL;
	if (current_head && state->current_bb &&
		state->current_bb->type & R_ANAL_BB_TYPE_TAIL) {
		r_anal_ex_update_bb_cfg_head_tail (current_head, current_head, state->current_bb);
		// XXX should i do this instead -> r_anal_ex_perform_post_anal_bb_cb (anal, state, addr+offset);
		state->done = 1;
	}
	return R_ANAL_RET_END;
}

static int java_recursive_descent(RAnal *anal, RAnalState *state, ut64 addr) {
	RAnalBlock *bb = state->current_bb;
	RAnalBlock *current_head = state->current_bb_head;
	if (current_head && state->current_bb->type & R_ANAL_BB_TYPE_TAIL) {
		r_anal_ex_update_bb_cfg_head_tail (current_head, current_head, state->current_bb);
	}

	// basic filter for handling the different type of operations
	// depending on flags some may be called more than once
	// if (bb->type2 & R_ANAL_EX_ILL_OP)   handle_bb_ill_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_COND_OP)  handle_bb_cond_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_UNK_OP)   handle_bb_unknown_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_NULL_OP)  handle_bb_null_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_NOP_OP)   handle_bb_nop_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_REP_OP)   handle_bb_rep_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_STORE_OP) handle_bb_store_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_LOAD_OP)  handle_bb_load_op (anal, state
	// if (bb->type2 & R_ANAL_EX_REG_OP)   handle_bb_reg_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_OBJ_OP)   handle_bb_obj_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_STACK_OP) handle_bb_stack_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_BIN_OP)   handle_bb_bin_op (anal, state);
	if (bb->type2 & R_ANAL_EX_CODE_OP)  handle_bb_cf_recursive_descent (anal, state);
	// if (bb->type2 & R_ANAL_EX_DATA_OP)  handle_bb_data_op (anal, state);
	return 0;
}

static int java_linear_sweep(RAnal *anal, RAnalState *state, ut64 addr) {
	RAnalBlock *bb = state->current_bb;
	if (state->current_bb_head && state->current_bb->type & R_ANAL_BB_TYPE_TAIL) {
		//r_anal_ex_update_bb_cfg_head_tail (state->current_bb_head, state->current_bb_head, state->current_bb);
	}

	// basic filter for handling the different type of operations
	// depending on flags some may be called more than once
	// if (bb->type2 & R_ANAL_EX_ILL_OP)   handle_bb_ill_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_COND_OP)  handle_bb_cond_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_UNK_OP)   handle_bb_unknown_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_NULL_OP)  handle_bb_null_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_NOP_OP)   handle_bb_nop_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_REP_OP)   handle_bb_rep_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_STORE_OP) handle_bb_store_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_LOAD_OP)  handle_bb_load_op (anal, state
	// if (bb->type2 & R_ANAL_EX_REG_OP)   handle_bb_reg_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_OBJ_OP)   handle_bb_obj_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_STACK_OP) handle_bb_stack_op (anal, state);
	// if (bb->type2 & R_ANAL_EX_BIN_OP)   handle_bb_bin_op (anal, state);
	if (bb->type2 & R_ANAL_EX_CODE_OP)  handle_bb_cf_linear_sweep (anal, state);
	// if (bb->type2 & R_ANAL_EX_DATA_OP)  handle_bb_data_op (anal, state);
	return 0;
}

static int handle_bb_cf_recursive_descent (RAnal *anal, RAnalState *state) {

	RAnalBlock *bb = state->current_bb;

	ut64 addr = 0;
	int result = 0;
	if (bb == NULL) {
		eprintf ("Error: unable to handle basic block @ 0x%08"PFMT64x"\n", addr);
		return R_ANAL_RET_ERROR;
	} else if (state->max_depth <= state->current_depth) {
		return R_ANAL_RET_ERROR;
	}

	state->current_depth++;
	addr = bb->addr;
	IFDBG eprintf ("Handling a control flow change @ 0x%04"PFMT64x".\n", addr);
	ut64 control_type = r_anal_ex_map_anal_ex_to_anal_op_type (bb->type2);

	// XXX - transition to type2 control flow condtions
	switch (control_type) {
		case R_ANAL_OP_TYPE_CALL:
			IFDBG eprintf (" - Handling a call @ 0x%04"PFMT64x".\n", addr);
			r_anal_fcn_xref_add (anal, state->current_fcn, bb->addr, bb->jump,
				control_type == R_ANAL_OP_TYPE_CALL? R_ANAL_REF_TYPE_CALL : R_ANAL_REF_TYPE_CODE);
			result = R_ANAL_RET_ERROR;
			break;
		case R_ANAL_OP_TYPE_JMP:
			{
				RList * jmp_list;
				IFDBG eprintf (" - Handling a jmp @ 0x%04"PFMT64x" to 0x%04"PFMT64x".\n", addr, bb->jump);

				// visited some other time
				if (r_anal_state_search_bb (state, bb->jump) == NULL) {
					jmp_list = r_anal_ex_perform_analysis ( anal, state, bb->jump );
					if (jmp_list)
						bb->jumpbb = (RAnalBlock *) r_list_get_n (jmp_list, 0);
				} else {
					bb->jumpbb = r_anal_state_search_bb (state, bb->jump);
				}

				if (state->done == 1) {
					IFDBG eprintf (" Looks like this jmp (bb @ 0x%04"PFMT64x") found a return.\n", addr);
				}
				result = R_ANAL_RET_END;
			}
			break;
		case R_ANAL_OP_TYPE_CJMP:
			{
				RList *jmp_list;
				ut8 encountered_stop = 0;
				IFDBG eprintf (" - Handling an cjmp @ 0x%04"PFMT64x" jmp to 0x%04"PFMT64x" and fail to 0x%04"PFMT64x".\n", addr, bb->jump, bb->fail);
				IFDBG eprintf (" - Handling jmp to 0x%04"PFMT64x".\n", bb->jump);
				// visited some other time
				if (r_anal_state_search_bb (state, bb->jump) == NULL) {
					jmp_list = r_anal_ex_perform_analysis ( anal, state, bb->jump );
					if (jmp_list)
						bb->jumpbb = (RAnalBlock *) r_list_get_n (jmp_list, 0);
				} else {
					bb->jumpbb = r_anal_state_search_bb (state, bb->jump);
				}

				if (state->done == 1) {
					IFDBG eprintf (" Looks like this jmp (bb @ 0x%04"PFMT64x") found a return.\n", addr);
					state->done = 0;
					encountered_stop = 1;
				}

				if (r_anal_state_search_bb (state, bb->fail) == NULL) {
					jmp_list = r_anal_ex_perform_analysis ( anal, state, bb->fail );
					if (jmp_list)
						bb->jumpbb = (RAnalBlock *) r_list_get_n (jmp_list, 0);
				} else {
					bb->jumpbb = r_anal_state_search_bb (state, bb->jump);
				}

				IFDBG eprintf (" - Handling an cjmp @ 0x%04"PFMT64x" jmp to 0x%04"PFMT64x" and fail to 0x%04"PFMT64x".\n", addr, bb->jump, bb->fail);
				IFDBG eprintf (" - Handling fail to 0x%04"PFMT64x".\n", bb->fail);
				// r_anal_state_merge_bb_list (state, fail_list);
				if (state->done == 1) {
					IFDBG eprintf (" Looks like this fail (bb @ 0x%04"PFMT64x") found a return.\n", addr);
				}

				result = R_ANAL_RET_END;
				if (encountered_stop) state->done = 1;
			}
			break;

		case R_ANAL_OP_TYPE_SWITCH:
		{
			IFDBG eprintf (" - Handling an switch @ 0x%04"PFMT64x".\n", addr);
			if (bb->switch_op) {
				RAnalCaseOp *caseop;
				RListIter *iter;
				RList *jmp_list = NULL;
				ut8 encountered_stop = 0;
				r_list_foreach (bb->switch_op->cases, iter, caseop) {
					if (caseop) {
						if (r_anal_state_addr_is_valid (state, caseop->jump) ) {
							jmp_list = r_anal_ex_perform_analysis ( anal, state, caseop->jump );
							if (jmp_list)
								caseop->jumpbb = (RAnalBlock *) r_list_get_n (jmp_list, 0);
							if (state->done == 1) {
								IFDBG eprintf (" Looks like this jmp (bb @ 0x%04"PFMT64x") found a return.\n", addr);
								state->done = 0;
								encountered_stop = 1;
							}
						}
					}
				}
				if (encountered_stop) state->done = 1;
			}

			result = R_ANAL_RET_END;
		}
			break;
		case R_ANAL_OP_TYPE_TRAP:
		case R_ANAL_OP_TYPE_UJMP:
		case R_ANAL_OP_TYPE_RET:
			IFDBG eprintf (" - Handling an ret @ 0x%04"PFMT64x".\n", addr);
			state->done = 1;
			result = R_ANAL_RET_END;
			break;
		default: break;
	}

	state->current_depth--;
	return result;
}

static int java_post_anal_linear_sweep(RAnal *anal, RAnalState *state, ut64 addr) {
	RAnalJavaLinearSweep *nodes = state->user_state;
	RList *jmp_list = NULL;
	ut64 *paddr64;

	state->done = 0;
	if (nodes == NULL || nodes->cfg_node_addrs == NULL) {
		state->done = 1;
		return R_ANAL_RET_ERROR;
	}

	while (r_list_length (nodes->cfg_node_addrs) > 0) {
		paddr64 = r_list_get_n (nodes->cfg_node_addrs, 0);
		r_list_del_n (nodes->cfg_node_addrs, 0);
		if (paddr64 && r_anal_state_search_bb (state, *paddr64) == NULL) {
			ut64 list_length = 0;
			IFDBG eprintf (" - Visiting 0x%04"PFMT64x" for analysis.\n", *paddr64);
			jmp_list = r_anal_ex_perform_analysis ( anal, state, *paddr64 );
			list_length = r_list_length (jmp_list);
			if ( list_length > 0) {
				IFDBG eprintf (" - Found %"PFMT64d" more basic blocks missed on the initial pass.\n", *paddr64);
			}
		}

	}
	return R_ANAL_RET_END;
}


static int handle_bb_cf_linear_sweep (RAnal *anal, RAnalState *state) {
	ut64 * paddr64;
	RAnalBlock *bb = state->current_bb;
	RAnalJavaLinearSweep *nodes = state->user_state;

	if (nodes == NULL || nodes->cfg_node_addrs == NULL) {
		state->done = 1;
		return R_ANAL_RET_ERROR;
	}

	ut64 addr = 0;
	int result = 0;
	if (bb == NULL) {
		eprintf ("Error: unable to handle basic block @ 0x%08"PFMT64x"\n", addr);
		return R_ANAL_RET_ERROR;
	} else if (state->max_depth <= state->current_depth) {
		return R_ANAL_RET_ERROR;
	}

	state->current_depth++;
	addr = bb->addr;
	IFDBG eprintf ("Handling a control flow change @ 0x%04"PFMT64x".\n", addr);
	ut32 control_type = r_anal_ex_map_anal_ex_to_anal_op_type (bb->type2);

	// XXX - transition to type2 control flow condtions
	switch (control_type) {
		case R_ANAL_OP_TYPE_CALL:
			IFDBG eprintf (" - Handling a call @ 0x%04"PFMT64x"\n", addr);
			r_anal_fcn_xref_add (anal, state->current_fcn, bb->addr, bb->jump,
				control_type == R_ANAL_OP_TYPE_CALL? R_ANAL_REF_TYPE_CALL : R_ANAL_REF_TYPE_CODE);
			result = R_ANAL_RET_ERROR;
			break;
		case R_ANAL_OP_TYPE_JMP:
			paddr64 = malloc (sizeof(ut64));
			*paddr64 = bb->jump;
			IFDBG eprintf (" - Handling a jmp @ 0x%04"PFMT64x", adding for future visit\n", addr);
			r_list_append (nodes->cfg_node_addrs, paddr64);
			result = R_ANAL_RET_END;
			break;
		case R_ANAL_OP_TYPE_CJMP:
			paddr64 = malloc (sizeof(ut64));
			*paddr64 = bb->jump;
			IFDBG eprintf (" - Handling a bb->jump @ 0x%04"PFMT64x", adding 0x%04"PFMT64x" for future visit\n", addr, *paddr64);
			r_list_append (nodes->cfg_node_addrs, paddr64);
			paddr64 = malloc (sizeof(ut64));
			*paddr64 = bb->fail;
			IFDBG eprintf (" - Handling a bb->fail @ 0x%04"PFMT64x", adding 0x%04"PFMT64x" for future visit\n", addr, *paddr64);
			r_list_append (nodes->cfg_node_addrs, paddr64);
			result = R_ANAL_RET_END;
			break;
		case R_ANAL_OP_TYPE_SWITCH:
			if (bb->switch_op) {
				RAnalCaseOp *caseop;
				RListIter *iter;
				//RList *jmp_list = NULL;
				IFDBG eprintf (" - Handling a switch_op @ 0x%04"PFMT64x":\n", addr);
				r_list_foreach (bb->switch_op->cases, iter, caseop) {
					ut64 * paddr64;
					if (caseop) {
						paddr64 = malloc (sizeof(ut64));
						*paddr64 = caseop->jump;
						IFDBG eprintf ("Adding 0x%04"PFMT64x" for future visit\n", *paddr64);
						r_list_append (nodes->cfg_node_addrs, paddr64);
					}
				}
			}
			result = R_ANAL_RET_END;
			break;
		case R_ANAL_OP_TYPE_TRAP:
		case R_ANAL_OP_TYPE_UJMP:
		case R_ANAL_OP_TYPE_RET:
			IFDBG eprintf (" - Handling an ret @ 0x%04"PFMT64x".\n", addr);
			state->done = 1;
			result = R_ANAL_RET_END;
			break;
		default: break;
	}

	state->current_depth--;
	return result;
}


static int analyze_from_code_buffer ( RAnal *anal, RAnalFunction *fcn, ut64 addr, const ut8 *code_buf, ut64 code_length  ) {

	char gen_name[1025];

	RAnalState *state = NULL;
	int result = R_ANAL_RET_ERROR;
	RAnalJavaLinearSweep *nodes;

	free (fcn->name);
	free (fcn->dsc);
	snprintf (gen_name, 1024, "sym.%08"PFMT64x"", addr);

	fcn->name = strdup (gen_name);
	fcn->dsc = strdup ("unknown");
	fcn->size = code_length;
	fcn->type = R_ANAL_FCN_TYPE_FCN;
	fcn->addr = addr;

	state = r_anal_state_new ( addr, (ut8 * ) code_buf, code_length);
	nodes = R_NEW0 (RAnalJavaLinearSweep);
	nodes->cfg_node_addrs = r_list_new ();
	nodes->cfg_node_addrs->free = free;

	state->user_state = nodes;

	result = analyze_method (anal, fcn, state);
	fcn->size = state->bytes_consumed;
	result = state->anal_ret_val;

	r_list_free (nodes->cfg_node_addrs);
	free (nodes);
	r_anal_state_free (state);
	IFDBG eprintf ("Completed analysing code from buffer, name: %s, desc: %s\n", fcn->name, fcn->dsc);

	return result;
}

static int analyze_from_code_attr (RAnal *anal, RAnalFunction *fcn, RBinJavaField *method, ut64 loadaddr) {
	RBinJavaAttrInfo* code_attr = method ? r_bin_java_get_method_code_attribute(method) : NULL;
	ut8 * code_buf = NULL;
	char * name_buf = NULL;
	int result = R_FALSE;

	ut64 code_length = 0,
		 code_addr = -1;


	if (code_attr == NULL) {
		char gen_name[1025];
		snprintf (gen_name, 1024, "sym.UNKNOWN");

		fcn->name = strdup (gen_name);
		fcn->dsc = strdup ("unknown");

		fcn->size = code_length;
		fcn->type = R_ANAL_FCN_TYPE_FCN;
		fcn->addr = 0;

		return R_ANAL_RET_ERROR;
	}

	code_length = code_attr->info.code_attr.code_length;
	code_addr = code_attr->info.code_attr.code_offset;

	code_buf = malloc (code_length);

	anal->iob.read_at (anal->iob.io, code_addr + loadaddr, code_buf, code_length);
	result = analyze_from_code_buffer ( anal, fcn, code_addr+loadaddr, code_buf, code_length);

	free (code_buf);

	name_buf = (char *) malloc (R_FLAG_NAME_SIZE);
	if (name_buf){
		char *cname = NULL;
		char *name = strdup(method->name);
		r_name_filter (name, 80);
		if (method->class_name) {
			cname = strdup (method->class_name);
			r_name_filter (cname, 50);
			sprintf (name_buf, "sym.%s.%s", cname, name);
		} else {
			sprintf (name_buf, "sym.%s", name);
		}
		free (fcn->name);
		fcn->name = strdup (name_buf);
		free (name_buf);
		free (cname);
		free (name);
	}

	free (fcn->dsc);
	fcn->dsc = strdup (method->descriptor);
	IFDBG eprintf ("Completed analysing code from attr, name: %s, desc: %s", fcn->name, fcn->dsc);

	return result;
}

static int analyze_method(RAnal *anal, RAnalFunction *fcn, RAnalState *state) {
	ut64 bytes_consumed = 0;
	RList *bbs = NULL;
	// deallocate niceties
	r_list_free (fcn->bbs);
	fcn->bbs = r_anal_bb_list_new ();

	IFDBG eprintf ("analyze_method: Parsing fcn %s @ 0x%08"PFMT64x", %d bytes\n", fcn->name, fcn->addr, fcn->size);	
	java_new_method (fcn->addr);
	state->current_fcn = fcn;
	// Not a resource leak.  Basic blocks should be stored in the state->fcn
	bbs = r_anal_ex_perform_analysis (anal, state, fcn->addr);
	bytes_consumed = state->bytes_consumed;
	IFDBG eprintf ("analyze_method: Completed Parsing fcn %s @ 0x%08"PFMT64x", consumed %"PFMT64d" bytes\n", fcn->name, fcn->addr, bytes_consumed);

	return state->anal_ret_val;
}

static int java_analyze_fns_from_buffer( RAnal *anal, ut64 start, ut64 end, int reftype, int depth) {

	int result = R_ANAL_RET_ERROR;
	ut64 addr = start;
	ut64 offset = 0;
	ut64 buf_len = end - start;
	ut8 analyze_all = 0,
		*buffer = NULL;

	if (end == UT64_MAX) {
		//analyze_all = 1;
		buf_len = anal->iob.size (anal->iob.io);

		if (buf_len == UT64_MAX) buf_len = 1024;

		end = start + buf_len;
	}


	buffer = malloc (buf_len);
	if (buffer == NULL) return R_ANAL_RET_ERROR;


	anal->iob.read_at (anal->iob.io, addr, buffer, buf_len);

	while (offset < buf_len) {
		ut64 length = buf_len - offset;

		RAnalFunction *fcn = r_anal_fcn_new ();
		result = analyze_from_code_buffer ( anal, fcn, addr, buffer+offset, length );
		if (result == R_ANAL_RET_ERROR) {
			eprintf ("Failed to parse java fn: %s @ 0x%04"PFMT64x"\n", fcn->name, fcn->addr);
			// XXX - TO Stop or not to Stop ??
			break;
		}
		//r_listrange_add (anal->fcnstore, fcn);
		r_list_append (anal->fcns, fcn);
		offset += fcn->size;
		if (!analyze_all) break;
	}
	free (buffer);
	return result;
}



static int java_analyze_fns( RAnal *anal, ut64 start, ut64 end, int reftype, int depth) {
	//anal->iob.read_at (anal->iob.io, op.jump, bbuf, sizeof (bbuf));
	RBinJavaObj *bin = NULL;// = get_java_bin_obj (anal);
	RBinJavaField *method = NULL;
	RListIter *methods_iter, *bin_obs_iter;

	RList * bin_objs_list = get_java_bin_obj_list (anal),
		  * methods_list = NULL;// = bin ? r_bin_java_get_methods_list (bin) : NULL;

	ut8 analyze_all = 0;
	//RAnalRef *ref = NULL;
	int result = R_ANAL_RET_ERROR;

	if (end == UT64_MAX) analyze_all = 1;

	if (bin_objs_list == NULL || r_list_length (bin_objs_list) == 0) {
		r_list_free (bin_objs_list);
		return java_analyze_fns_from_buffer (anal, start, end, reftype, depth);
	}

	r_list_foreach (bin_objs_list, bin_obs_iter, bin) {
		// loop over all bin object that are loaded
		java_update_anal_types (anal, bin);
		methods_list = (RList *) r_bin_java_get_methods_list (bin);
		if (methods_list) {
			ut64 loadaddr = bin->loadaddr;
			const char * bin_name = bin && bin->file ? bin->file : anal->iob.io->fd->name;
			IFDBG eprintf ("Analyzing java functions for %s\n", bin_name);
			IFDBG eprintf ("Analyzing functions.  binobj = %p, methods_list = %p, Analysing from buffer? %d\n", bin, methods_list, methods_list == NULL);
			// loop over all methods in the binary object and analyse
			// the functions
			r_list_foreach ( methods_list, methods_iter, method ) {
				if ( (method && analyze_all) ||
					(check_addr_less_start (method, end) ||
					check_addr_in_code (method, end)) ) {

					RAnalFunction *fcn = r_anal_fcn_new ();
					java_set_function_prototype (anal, fcn, method);
					result = analyze_from_code_attr ( anal, fcn, method, loadaddr );
					if (result == R_ANAL_RET_ERROR) {
						eprintf ("Failed to parse java fn: %s @ 0x%04"PFMT64x"\n", fcn->name, fcn->addr);
						// XXX - TO Stop or not to Stop ??
					}
					//r_listrange_add (anal->fcnstore, fcn);
					r_list_append (anal->fcns, fcn);
				}
			} // End of methods loop
		}// end of methods_list is valid conditional
	}// end of bin_objs list loop
	return result;
}

/*static int java_fn(RAnal *anal, RAnalFunction *fcn, ut64 addr, ut8 *buf, ut64 len, int reftype) {
	// XXX - this may clash with malloc:// uris because the file name is
	// malloc:// **
	RBinJavaObj *bin = (RBinJavaObj *) get_java_bin_obj (anal);
	RBinJavaField *method = bin ? r_bin_java_get_method_code_attribute_with_addr (bin,  addr) : NULL;
	ut64 loadaddr = bin ? bin->loadaddr : 0;
	IFDBG eprintf ("Analyzing java functions for %s\n", anal->iob.io->fd->name);
	if (method) return analyze_from_code_attr (anal, fcn, method, loadaddr);
	return analyze_from_code_buffer (anal, fcn, addr, buf, len);
}*/

static int java_switch_op(RAnal *anal, RAnalOp *op, ut64 addr, const ut8 *data, int len) {
	ut8 op_byte = data[0];
	ut64 offset = addr - java_get_method_start ();
	ut8 pos = (offset+1)%4 ? 1 + 4 - (offset+1)%4 : 1;


	if (op_byte == 0xaa) {
		// handle a table switch condition
		int min_val = (ut32)(UINT (data, pos + 4)),
			max_val = (ut32)(UINT (data, pos + 8));

		ut32 default_loc = (ut32)(UINT (data, pos)),
			 cur_case = 0;

		op->switch_op = r_anal_switch_op_new (addr, min_val, default_loc);

		RAnalCaseOp *caseop = NULL;
		IFDBG {
			eprintf ("Handling tableswitch op @ 0x%04"PFMT64x"\n", addr);
			eprintf ("default_jump @ 0x%04"PFMT64x"\n", default_loc+addr);
			eprintf ("min_val: %d max_val: %d\n", min_val, max_val);
		}
		pos += 12;

		//caseop = r_anal_switch_op_add_case(op->switch_op, addr+default_loc, -1, addr+offset);
		for (cur_case = 0; cur_case <= max_val - min_val; pos+=4, cur_case++) {
			//ut32 value = (ut32)(UINT (data, pos));
			ut32 offset = (ut32)(R_BIN_JAVA_UINT (data, pos));
			IFDBG eprintf ("offset value: 0x%04x, interpretted addr case: %d offset: 0x%04"PFMT64x"\n", offset, cur_case+min_val, addr+offset);
			caseop = r_anal_switch_op_add_case (op->switch_op, addr+pos, cur_case+min_val, addr+offset);
			caseop->bb_ref_to = addr+offset;
			caseop->bb_ref_from = addr; // TODO figure this one out
		}
	}
	op->size = pos;
	return op->size;
}
static int java_op(RAnal *anal, RAnalOp *op, ut64 addr, const ut8 *data, int len) {
	int sz = 1;

	/* get opcode size */
	//ut8 op_byte = data[0];
	ut8 op_byte = data[0];
	sz = JAVA_OPS[op_byte].size;
	if (op == NULL)	return sz;

	memset (op, '\0', sizeof (RAnalOp));

	IFDBG {
		//eprintf ("Extracting op from buffer (%d bytes) @ 0x%04x\n", len, addr);
		//eprintf ("Parsing op: (0x%02x) %s.\n", op_byte, JAVA_OPS[op_byte].name);
	}
	op->addr = addr;
	op->size= sz;
	op->type2 = JAVA_OPS[op_byte].op_type;
	op->type = r_anal_ex_map_anal_ex_to_anal_op_type (op->type2);
	// handle lookup and table switch offsets
	if (op_byte == 0xaa || op_byte == 0xab) {
		java_switch_op (anal, op, addr, data, len);
	}

	op->eob = r_anal_ex_is_op_type_eop (op->type2);
	IFDBG {
		char *ot_str = r_anal_optype_to_string (op->type);
		eprintf ("op_type2: %s @ 0x%04"PFMT64x" 0x%08"PFMT64x" op_type: (0x%02"PFMT64x") %s.\n", JAVA_OPS[op_byte].name, addr, op->type2, op->type,  ot_str);
		//eprintf ("op_eob: 0x%02x.\n", op->eob);
		//eprintf ("op_byte @ 0: 0x%02x op_byte @ 0x%04x: 0x%02x.\n", data[0], addr, data[addr]);
	}

	if ( op->type == R_ANAL_OP_TYPE_CJMP ) {
		op->jump = addr + (int)(short)(USHORT (data, 1));
		op->fail = addr + sz;
		IFDBG eprintf ("%s jmpto 0x%04"PFMT64x"  failto 0x%04"PFMT64x".\n", JAVA_OPS[op_byte].name, op->jump, op->fail);
	} else if ( op->type  == R_ANAL_OP_TYPE_JMP ) {
		op->jump = addr + (int)(short)(USHORT (data, 1));
		IFDBG eprintf ("%s jmpto 0x%04"PFMT64x".\n", JAVA_OPS[op_byte].name, op->jump);
	} else if ( (op->type & R_ANAL_OP_TYPE_CALL) == R_ANAL_OP_TYPE_CALL ) {
		//op->jump = addr + (int)(short)(USHORT (data, 1));
		//op->fail = addr + sz;
		//IFDBG eprintf ("%s callto 0x%04x  failto 0x%04x.\n", JAVA_OPS[op_byte].name, op->jump, op->fail);
	}

	//r_java_disasm(addr, data, output, outlen);
	//IFDBG eprintf ("%s\n", output);
	return op->size;
}
/*
static RAnalOp * java_op_from_buffer(RAnal *anal, RAnalState *state, ut64 addr) {

	RAnalOp *op = r_anal_op_new ();
	//  get opcode size 
	if (op == NULL) return 0;
	memset (op, '\0', sizeof (RAnalOp));
	java_op (anal, op, addr, state->buffer, state->len - (addr - state->start) );
	return op;

}
*/
static int java_print_method_definitions ( RBinJavaObj *obj ) {
	RList * the_list = r_bin_java_get_method_definitions (obj),
			* off_list = r_bin_java_get_method_offsets (obj);
	char * str = NULL;
	ut32 idx = 0, end = r_list_length (the_list);

	while (idx < end) {
		ut64 *addr = r_list_get_n (off_list, idx);
		str = r_list_get_n (the_list, idx);
		eprintf("%s; // @0x%04"PFMT64x"\n", str, *addr);
		idx++;
	}

	r_list_free(the_list);
	r_list_free(off_list);
	return 0;
}

static int java_print_field_definitions ( RBinJavaObj *obj ) {
	RList * the_list = r_bin_java_get_field_definitions (obj),
			* off_list = r_bin_java_get_field_offsets (obj);
	char * str = NULL;
	ut32 idx = 0, end = r_list_length (the_list);

	while (idx < end) {
		ut64 *addr = r_list_get_n (off_list, idx);
		str = r_list_get_n (the_list, idx);
		eprintf("%s; // @0x%04"PFMT64x"\n", str, *addr);
		idx++;
	}

	r_list_free(the_list);
	r_list_free(off_list);
	return 0;
}

static int java_print_import_definitions ( RBinJavaObj *obj ) {
	RList * the_list = r_bin_java_get_import_definitions (obj);
	char * str = NULL;
	RListIter *iter;
	r_list_foreach (the_list, iter, str) {
		eprintf("import %s;\n", str);
	}
	r_list_free(the_list);
	return 0;
}

static int java_print_all_definitions( RAnal *anal ) {
	RList * obj_list  = get_java_bin_obj_list (anal);
	RListIter *iter;
	RBinJavaObj *obj;

	if (!obj_list) return 1;
	r_list_foreach (obj_list, iter, obj) {
		java_print_class_definitions (obj);
	}
	return 0;
}
static int java_print_class_definitions( RBinJavaObj *obj ) {
	RList * the_fields = r_bin_java_get_field_definitions (obj),
			* the_methods = r_bin_java_get_method_definitions (obj),
			* the_imports = r_bin_java_get_import_definitions (obj),
			* the_moffsets = r_bin_java_get_method_offsets (obj),
			* the_foffsets = r_bin_java_get_field_offsets (obj);

	char * class_name = r_bin_java_get_this_class_name(obj),
		 * str = NULL;

	java_print_import_definitions (obj);
	eprintf ("\nclass %s { // @0x%04"PFMT64x"\n", class_name, obj->loadaddr);

	if (the_fields && the_foffsets && r_list_length (the_fields) > 0) {
		eprintf ("\n\t// Fields defined in the class\n");
		ut32 idx = 0, end = r_list_length (the_fields);

		while (idx < end) {
			ut64 *addr = r_list_get_n (the_foffsets, idx);
			str = r_list_get_n (the_fields, idx);
			eprintf("\t%s; // @0x%04"PFMT64x"\n", str, *addr);
			idx++;
		}
	}

	if (the_methods && the_moffsets && r_list_length (the_methods) > 0) {
		eprintf ("\n\t// Methods defined in the class\n");
		ut32 idx = 0, end = r_list_length (the_methods);

		while (idx < end) {
			ut64 *addr = r_list_get_n (the_moffsets, idx);
			str = r_list_get_n (the_methods, idx);
			eprintf("\t%s; // @0x%04"PFMT64x"\n", str, *addr);
			idx++;
		}
	}
	eprintf ("}\n");

	r_list_free (the_imports);
	r_list_free (the_fields);
	r_list_free (the_methods);
	r_list_free (the_foffsets);
	r_list_free (the_moffsets);

	free(class_name);
	return 0;
}

static int java_print_class_access_flags_value( const char * flags ){
	ut16 result = r_bin_java_calculate_class_access_value (flags);
	eprintf ("Access Value for %s = 0x%04x\n", flags, result);
	return 0;
}
static int java_print_field_access_flags_value( const char * flags ){
	ut16 result = r_bin_java_calculate_field_access_value (flags);
	eprintf ("Access Value for %s = 0x%04x\n", flags,  result);
	return 0;
}
static int java_print_method_access_flags_value( const char * flags ){
	ut16 result = r_bin_java_calculate_method_access_value (flags);
	eprintf ("Access Value for %s = 0x%04x\n", flags,  result);
	return 0;
}

static void java_set_function_prototype (RAnal *anal, RAnalFunction *fcn, RBinJavaField *method) {
	RList *the_list = r_bin_java_extract_type_values (method->descriptor);
	Sdb *D = anal->sdb_types;
	Sdb *A = anal->sdb_args;
	const char *type_fmt = "%08"PFMT64x".arg.%d.type",
	     *namek_fmt = "%08"PFMT64x".var.%d.name",
	     *namev_fmt = "%08"PFMT64x"local.%d";

	char  key_buf[1024], value_buf [1024];
	RListIter *iter;
	char *str;

	if (the_list) {
		ut8 start = 0, stop = 0;
		int idx = 0;
		r_list_foreach (the_list, iter, str) {
			IFDBG eprintf ("Adding type: %s to known types.\n", str);
			if (str && *str == '('){
				start = 1;
				continue;
			}

			if (str && start && *str != ')') {
				// set type
				// set arg type
				snprintf (key_buf, sizeof(key_buf)-1, type_fmt, (ut64)fcn->addr, idx);
				sdb_set (A, str, key_buf, 0);
				sdb_set (D, str, "type", 0);
				// set value
				snprintf (key_buf, sizeof(key_buf)-1, namek_fmt, fcn->addr, idx);
				snprintf (value_buf, sizeof(value_buf)-1, namev_fmt, fcn->addr, idx);
				sdb_set (A, value_buf, key_buf, 0);
				idx ++;
			}
			if (start && str && *str == ')') {
				stop = 1;
				continue;
			}

			if ( (start & stop & 1) && str ){
				sdb_set (A, str, "ret.type", 0);
				sdb_set (D, str, "type", 0);
			}
		}
		r_list_free (the_list);
	}
}

static void java_update_anal_types (RAnal *anal, RBinJavaObj *bin_obj) {
	Sdb *D = anal->sdb_types;
	if (D && bin_obj) {
		RListIter *iter;
		char *str;
		RList * the_list = r_bin_java_extract_all_bin_type_values (bin_obj);
		if (the_list) {
			r_list_foreach (the_list, iter, str) {
				IFDBG eprintf ("Adding type: %s to known types.\n", str);
				if (str) sdb_set (D, str, "type", 0);
			}
		}
		r_list_free (the_list);
	}
}

static int java_resolve_cp_idx (RAnal *anal, const char * idxs) {
	RBinJavaObj *obj = get_java_bin_obj (anal);
	char *str = NULL;
	ut16 idx = atoi (idxs); // XXX - Bug more checking needed
	if (obj && idx){
		str = r_bin_java_resolve_without_space (obj, idx);
		eprintf ("%s\n", str);
		free (str);
	}
	return R_TRUE;
}

static int java_resolve_cp_type (RAnal *anal, const char * idxs) {
	RBinJavaObj *obj = get_java_bin_obj (anal);
	char *str = NULL;
	ut16 idx = atoi (idxs); // XXX - Bug more checking needed
	if (obj && idx){
		str = r_bin_java_resolve_cp_idx_type (obj, idx);
		eprintf ("%s\n", str);
		free (str);
	}
	return R_TRUE;
}

static int java_resolve_cp_idx_b64 (RAnal *anal, const char * idxs) {
	RBinJavaObj *obj = get_java_bin_obj (anal);
	char *str = NULL;
	ut16 idx = atoi (idxs); // XXX - Bug more checking needed
	if (obj && idx){
		str = r_bin_java_resolve_b64_encode (obj, idx) ;
		eprintf ("%s\n", str);
		free (str);
	}
	return R_TRUE;
}

static int java_cmd_ext(RAnal *anal, const char* input) {
	RBinJavaObj *obj = (RBinJavaObj *) get_java_bin_obj (anal);

	if (!obj) {
		eprintf ("Execute \"af\" to set the current bin, and this will bind the current bin\n");
		return -1;
	}
	switch (*input) {
		case 'c':
			// reset bytes counter for case operations
			r_java_new_method ();
			break;
		case 'u':
			switch (*(input+1)) {
				case 't': {java_update_anal_types (anal, obj); return R_TRUE;}
				default: break;
			}
			break;
		case 'p':
			switch (*(input+1)) {
				case 'm': return java_print_method_definitions (obj);
				case 'f': return java_print_field_definitions (obj);
				case 'i': return java_print_import_definitions (obj);
				case 'c': return java_print_class_definitions (obj);
				case 'a': return java_print_all_definitions (anal);
				default: break;
			}
			break;
		case 'f':
			switch (*(input+1)) {
				case 'm': return java_print_method_access_flags_value (input+2);
				case 'f': return java_print_field_access_flags_value (input+2);
				case 'c': return java_print_class_access_flags_value (input+2);
				default: break;
			}
			break;
		case 'r':
			switch (*(input+1)) {
				case 't': return java_resolve_cp_type (anal, input+2);
				case 'c': return java_resolve_cp_idx (anal, input+2);
				case 'e': return java_resolve_cp_idx_b64 (anal, input+2);
				default: break;
			}
			break;
		default: eprintf("Command not supported"); break;
	}
	return 0;
}

static int java_reset_counter (RAnal *anal, ut64 start_addr ) {
	IFDBG eprintf ("Setting the new METHOD_START to 0x%08"PFMT64x" was 0x%08"PFMT64x"\n", start_addr, METHOD_START);
	METHOD_START = start_addr;
	r_java_new_method ();
	return R_TRUE;
}

struct r_anal_plugin_t r_anal_plugin_java = {
	.name = "java",
	.desc = "Java bytecode analysis plugin",
	.license = "Apache",
	.arch = R_SYS_ARCH_JAVA,
	.bits = 32,
	.init = NULL,
	.fini = NULL,
	.custom_fn_anal = 1,

	.reset_counter = java_reset_counter,
	.analyze_fns = java_analyze_fns,
	.post_anal_bb_cb = java_recursive_descent,
	.revisit_bb_anal = java_revisit_bb_anal_recursive_descent,
	.op = &java_op,
	.bb = NULL,
	.fcn = NULL,

	.op_from_buffer = NULL,
	.bb_from_buffer = NULL,
	.fn_from_buffer = NULL,
	.cmd_ext = java_cmd_ext,


	.set_reg_profile = NULL,
	.fingerprint_bb = NULL,
	.fingerprint_fcn = NULL,
	.diff_bb = NULL,
	.diff_fcn = NULL,
	.diff_eval = NULL,

};

struct r_anal_plugin_t r_anal_plugin_java_ls = {
	.name = "java_ls",
	.desc = "Java bytecode analysis plugin with linear sweep",
	.license = "Apache",
	.arch = R_SYS_ARCH_JAVA,
	.bits = 32,
	.init = NULL,
	.fini = NULL,
	.custom_fn_anal = 1,

	.analyze_fns = java_analyze_fns,
	.post_anal_bb_cb = java_linear_sweep,
	.post_anal = java_post_anal_linear_sweep,
	.revisit_bb_anal = java_revisit_bb_anal_recursive_descent,
	.op = &java_op,
	.bb = NULL,
	.fcn = NULL,

	.op_from_buffer = NULL,
	.bb_from_buffer = NULL,
	.fn_from_buffer = NULL,


	.set_reg_profile = NULL,
	.fingerprint_bb = NULL,
	.fingerprint_fcn = NULL,
	.diff_bb = NULL,
	.diff_fcn = NULL,
	.diff_eval = NULL,

	.cmd_ext = java_cmd_ext,

};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_ANAL,
	//.data = &r_anal_plugin_java
	.data = &r_anal_plugin_java_ls
};
#endif
