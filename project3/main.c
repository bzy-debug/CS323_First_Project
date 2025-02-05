#include<stdio.h>
#include<string.h>
#include"syntax.tab.c"
#include"llist.h"
#include"node.h"
#include"type.h"
#include"symbol_table.h"
#include"semantic_error.h"
#include"inter_code.h"
#include<stdarg.h>

int x = 0;
int y = 0;

void generate_grammar_tree(FILE *);
void semantic_check(node* grammar_tree, llist* symbol_table);
void init(node* grammar_tree);

void test(inter_code* start) {
    inter_code* cur = start->next;
    while (cur != NULL )
    {
        print_code(cur) ;
        cur = cur->next;
    }
    printf("\n");
}

inter_code* my_generate_IR(node* grammar_tree);
inter_code*  generate_IR(node* grammar_tree);
inter_code* translate_Exp(node* Exp, char* place);
inter_code* translate_cond_Exp(node* Exp,int  lb1,int  lb2);
inter_code* translate_Stmt(node* Stmt);
inter_code* translate_Arg(node* Arg);
inter_code* translate_FunDec(node* Fundec);
inter_code* translate_VarList(node* VarList);
inter_code* translate_VarDec(node* vardec);
inter_code* translate_Dec(node* dec);
inter_code* translate_StructSpecifier(node* Struct_S);
inter_code* translate_exp_addr(node* exp,char* addr,int base);
inter_code* translate_DefList(node* DefList);


inter_code* ir_concatenate(int num,...);

char* new_place();
int  new_lable();

int main(int argc, char**argv) {
    if (argc <= 1)
        return 1;
    int l = strlen(argv[1]);
    char* outa = malloc(sizeof(char)*(l+1));
    strcpy(outa, argv[1]);
    outa[l-3]='i'; outa[l-2]='r'; outa[l-1]='\0';
    freopen(outa, "w", stdout);
    FILE *f = fopen(argv[1], "r");
    if (!f){
        perror(argv[1]);
        return 1;
    }

    generate_grammar_tree(f);
    // generate_IR(root);
    fclose(f);

    if(iserror == 0){
        // print_tree(root, 0);
    }
    else
        return 1;

    llist* sc_symbol_table_stack = create_llist();     //value: symbol_table

    

    semantic_check(root, sc_symbol_table_stack);
    
    // init(root);

    // llist* ir_symbol_table_stack = create_llist();
    inter_code* start = my_generate_IR(root);
    test(start);
    
    

    // llist_node* cur = symbol_table_stack->head->next;
    // while (cur != symbol_table_stack->tail)
    // {
    //     print_symbol_table(cur->value);
    //     cur = cur->next;
    // }

    return 0;
}

void init(node* grammar_tree) {
    llist* stack = create_llist(NULL);
    llist_append(stack, create_node(NULL, grammar_tree));
    while (stack->size >= 1)
    {
        node* pare = (node*)llist_pop(stack)->value;

        pare->isexplored = 0;

        llist_node* cur = pare->children->tail->prev;
        while (cur != pare->children->head)
        {
            llist_append(stack, create_node(NULL, cur->value));
            cur = cur->prev;
        }
    }
    
}

void generate_grammar_tree(FILE* f) {
    yylineno = 1;
    // yydebug = 1;
    yyrestart(f);
    yyparse();
}

void semantic_check(node* grammar_tree, llist* symbol_table_stack) {
    MyType* read_func = createType("func");
    setFuncReturnType(read_func, createType("int"));

    MyType* write_func = createType("func");
    addFuncParameter(write_func, createType("int"), "x");

    llist* symbol_table = create_llist();
    llist_append(symbol_table_stack, create_node(NULL, symbol_table));
    llist_append(symbol_table, create_node("read", read_func));
    llist_append(symbol_table, create_node("write", write_func));

    llist* stack = create_llist(NULL);
    llist_append(stack, create_node(NULL, grammar_tree));
    while (stack->size >= 1)
    {
        node* pare = (node*)llist_pop(stack)->value;

        if(pare->node_type == eRC && pare->pare->node_type == nterm && strcmp(pare->pare->val.ntermval,"CompSt") == 0) {
            llist_pop(symbol_table_stack);
            symbol_table = llist_peak(symbol_table_stack)->value;
        }

        if(pare->isempty || pare->children == NULL)   continue;

        else if(pare->node_type == nterm && strcmp(pare->val.ntermval,"CompSt") == 0 ) {
            symbol_table = create_llist();
            llist_append(symbol_table_stack, create_node(NULL, symbol_table));

            if(strcmp(pare->pare->val.ntermval, "ExtDef") == 0) {
                node* func_node = (node*)pare->pare->children->head->next->next->value;
                char* func_id = ((node*)func_node->children->head->next->value)->val.idval;

                if(symbol_table_contains_func(symbol_table_stack, func_id)) {
                    semantic_error(4, func_node->line, func_id);
                }

                MyType* func_type = get_type_by_key(func_id, symbol_table_stack);
                llist_concatenate(symbol_table, get_func_parameter(func_type));
            }
        }

        else if(pare->node_type == nterm && strcmp(pare->val.ntermval,"Def") == 0 && pare->isexplored == 0) {
            llist* t = get_symbol_node_list_from_def(pare, symbol_table, symbol_table_stack);
            
            llist_node* cur = t->head->next;
            while (cur != t->tail)
            {
                // redefine 仅限当前symbol_table
                if(llist_contains(symbol_table, cur->key)) {
                    semantic_error(3, pare->line, cur->key);
                }
                cur = cur->next;
            }
            
            llist_concatenate(symbol_table, t);
        }

        else if (pare->node_type == nterm && strcmp(pare->val.ntermval,"ExtDef") == 0 && pare->isexplored == 0) {
            llist* t = get_symbol_node_list_from_extdef(pare, symbol_table, symbol_table_stack);
            llist_concatenate(symbol_table, t);
            // print_symbol_table(symbol_table);
        }

        else if (pare->node_type == nterm && strcmp(pare->val.ntermval, "Exp") == 0 && pare->isexplored == 0) {
            get_exp_type(pare, symbol_table_stack);
        }

        else if (pare->node_type == nterm && strcmp(pare->val.ntermval, "Stmt") == 0 && pare->children->size == 3) {
            MyType* current_function = get_current_function(symbol_table_stack);
            node* exp = (node*) pare->children->head->next->next->value;
            MyType* expected_return_type = current_function->function->returnType;
            MyType* actual_return_type = get_exp_type(exp, symbol_table_stack);
            if(actual_return_type == NULL) {
                semantic_error(8, exp->line, "");
            }
            else if (typeEqual(expected_return_type, actual_return_type) == -1) {
                semantic_error(8, exp->line, "");
            }
        }

        llist_node* cur = pare->children->tail->prev;
        while (cur != pare->children->head)
        {
            llist_append(stack, create_node(NULL, cur->value));
            cur = cur->prev;
        }
    } 
}

inter_code* my_generate_IR(node* grammar_tree) {
    inter_code* start_code = cnt_ic(1, 0);
    inter_code* cur_code = start_code;

    llist* stack = create_llist(NULL);
    llist_append(stack, create_node(NULL, grammar_tree));
    while (stack->size >= 1)
    {
        node* pare = (node*)llist_pop(stack)->value;
        if(pare->isempty || pare->children == NULL)   continue;
        // print_node(pare);

        if (pare->node_type == nterm && strcmp(pare->val.ntermval, "Stmt") == 0 && pare->istranslated == 0) {
            inter_code* ir = translate_Stmt(pare);
            if (ir != NULL) {
                ir_concatenate(2, cur_code, ir);
                while (cur_code->next != NULL)
                    cur_code = cur_code->next;
                
            // test(start_code);
            }
        }
        else if (pare->node_type == nterm && strcmp(pare->val.ntermval, "FunDec") == 0) {
            inter_code* ir = translate_FunDec(pare);
            if (ir != NULL) {
                cur_code = ir_concatenate(2, cur_code, ir);
                while (cur_code->next != NULL)
                    cur_code = cur_code->next;
            // test(start_code);
            }
        }
        else if (pare->node_type == nterm && strcmp(pare->val.ntermval, "Dec") == 0) {
            inter_code* ir = translate_Dec(pare);
            if (ir != NULL) {
                cur_code = ir_concatenate(2 , cur_code, ir);
                while (cur_code->next != NULL)
                    cur_code = cur_code->next;
            // test(start_code);
            }
        }
        // else if (pare->node_type == nterm && strcmp(pare->val.ntermval, "StructSpecifier") == 0) {
        //     inter_code* ir = translate_StructSpecifier(pare);
        //     if (ir != NULL) {
        //         cur_code = ir_concatenate(2 , cur_code, ir);
        //         while (cur_code->next != NULL)
        //             cur_code = cur_code->next;
        //     // test(start_code);
        //     }
        // }



        llist_node* cur = pare->children->tail->prev;
        while (cur != pare->children->head)
        {
            llist_append(stack, create_node(NULL, cur->value));
            cur = cur->prev;
        }
    } 
    return start_code;
}

inter_code* generate_IR(node* grammar_tree){

    llist* stack = create_llist(NULL);
    llist_append(stack, create_node(NULL, grammar_tree));
    while (stack->size >= 1)
    {
        node* pare = (node*)llist_pop(stack)->value;
        if (pare->node_type == nterm && strcmp(pare->val.ntermval, "Stmt") == 0) {
             inter_code*  ir = translate_Stmt(pare);
            if (ir != NULL){
                print_code(ir);
            }
            
        }
        else if (pare->node_type == nterm && strcmp(pare->val.ntermval, "FunDec") == 0) {
            inter_code*  ir = translate_FunDec(pare);
            if (ir != NULL){
                print_code(ir);
            }        
            }
        else if (pare->node_type == nterm && strcmp(pare->val.ntermval, "Dec") == 0) {
            inter_code*  ir = translate_Dec(pare);
            if (ir != NULL){
                print_code(ir);
            }        
        }

        if(pare->isempty || pare->children == NULL)   continue;

        llist_node* cur = pare->children->tail->prev;
        while (cur != pare->children->head)
        {
            llist_append(stack, create_node(NULL, cur->value));
            cur = cur->prev;
        }
    } 
}

inter_code* translate_Exp(node* Exp, char* place){

        node* pare = (node*)Exp->children->head->next->value;
        //table 1
        if(pare->node_type == eINT&& Exp->children->size == 1) {
            int value = pare->val.intval;

            return cnt_ic(cASSIGN, 2,cnt_op_str(VARIABLE,place), cnt_op_int(CONSTANT, value));
        }
        else if(pare->node_type == eID&& pare->pare->children->size == 1) {
            char* value = pare->val.idval;
            return cnt_ic(cASSIGN, 2,cnt_op_str(VARIABLE,place), cnt_op_str(VARIABLE, value));
        }
        else if(pare->node_type ==nterm&& strcmp(pare->val.ntermval, "Exp") == 0 && pare->pare->children->size == 3) {
            node* second_node = (node*)pare->pare->children->head->next->next->value;
            node* third_node =  (node*)pare->pare->children->head->next->next->next->value;
            if(second_node->node_type == eASSIGN){
                if (pare->children->size == 1){
                    node* exp1_id_node = (node*)pare->children->head->next->value;
                    char*value = exp1_id_node->val.idval;
                    char* tp= new_place();
                    inter_code* code1 = translate_Exp(third_node,tp);
                    inter_code* code2 = cnt_ic(cASSIGN, 2,cnt_op_str(VARIABLE,value), cnt_op_str(VARIABLE,tp));
                    inter_code* code3 = cnt_ic(cASSIGN, 2,cnt_op_str(VARIABLE,place), cnt_op_str(VARIABLE,value));
                    return ir_concatenate(3,code1,code2,code3);
                }else{
                    char* t1= new_place();
                    inter_code* code1 = translate_Exp(third_node,t1);
                    char* t2 = new_place();
                    inter_code* code2 = translate_exp_addr(pare,t2,4);
                    inter_code* code3 = cnt_ic(LEFT_S, 2,cnt_op_str(VARIABLE,t2), cnt_op_str(VARIABLE,t1));
                    return ir_concatenate(3,code1,code2,code3);
                }
                 
            }
            else if(second_node->node_type == ePLUS){
                char* t1= new_place();
                char* t2 = new_place();
                inter_code* code1 = translate_Exp(pare,t1);
                inter_code* code2 = translate_Exp(third_node,t2);
                inter_code* code3 = cnt_ic(cADD, 3, cnt_op_str(VARIABLE, t1), cnt_op_str(VARIABLE, t2), cnt_op_str(VARIABLE,place));
                return ir_concatenate(3,code1,code2,code3);
            }else if(second_node->node_type == eDOT){
                //Exp DOT ID
                char* addr =new_place();
                inter_code* code1 = translate_exp_addr(Exp,addr,0);
                return  ir_concatenate(2, code1, cnt_ic(RIGHT_S,2,cnt_op_str(VARIABLE,place),cnt_op_str(VARIABLE,addr)));
                
            }
            else if(second_node->node_type == eMUL){
                char* t1= new_place();
                char* t2 = new_place();
                inter_code* code1 = translate_Exp(pare,t1);
                inter_code* code2 = translate_Exp(third_node,t2);
                inter_code* code3 = cnt_ic(cMUL, 3, cnt_op_str(VARIABLE, t1), cnt_op_str(VARIABLE, t2), cnt_op_str(VARIABLE,place));
                return ir_concatenate(3,code1,code2,code3);
            }
            else if(second_node->node_type == eDIV){
                char* t1= new_place();
                char* t2 = new_place();
                inter_code* code1 = translate_Exp(pare,t1);
                inter_code* code2 = translate_Exp(third_node,t2);
                inter_code* code3 = cnt_ic(cDIV, 3, cnt_op_str(VARIABLE, t1), cnt_op_str(VARIABLE, t2), cnt_op_str(VARIABLE,place));
                return ir_concatenate(3,code1,code2,code3);
            }
            else if(second_node->node_type == eMINUS){
                char* t1= new_place();
                char* t2 = new_place();
                inter_code* code1 = translate_Exp(pare,t1);
                inter_code* code2 = translate_Exp(third_node,t2);
                inter_code* code3 = cnt_ic(cSUB, 3, cnt_op_str(VARIABLE, t1), cnt_op_str(VARIABLE, t2), cnt_op_str(VARIABLE,place));
                return ir_concatenate(3,code1,code2,code3);
            }

        }else if(pare->node_type ==eMINUS && pare->pare->children->size == 2) {
            node* second_node = (node*)pare->pare->children->head->next->next->value;
            if(second_node->node_type ==nterm&& strcmp(second_node->val.ntermval, "Exp") == 0){
                char* tp = new_place();
                inter_code* code1 = translate_Exp(second_node, tp);
                inter_code* code2 = cnt_ic(cSUB, 3, cnt_op_int(CONSTANT, 0), cnt_op_str(VARIABLE, tp), cnt_op_str(VARIABLE,place));
                return ir_concatenate(2,code1,code2);
            }
        }
        else if(pare->node_type ==nterm&& strcmp(pare->val.ntermval, "Exp") == 0 && pare->pare->children->size == 4) {
            //Exp1 LB Exp2 RB : read the arr value
            char* t1 = new_place();
            inter_code* code = translate_exp_addr(Exp,t1,4);
            return  ir_concatenate(2, code, cnt_ic(RIGHT_S,2,cnt_op_str(VARIABLE,place),cnt_op_str(VARIABLE,t1)));
        }
        else if(pare->node_type ==eLP) {
            //LP EXP RP
            node* second_node = (node*)Exp->children->head->next->next->value;
            return translate_Exp(second_node,place);
        }

        //table 4
        if(pare->node_type == eREAD){
            return cnt_ic(cREAD,1,cnt_op_str(VARIABLE,place));
        }else if(pare->node_type == eWRITE){
            char* tp = new_place();
            node* exp_node = (node*)pare->pare->children->head->next->next->next->value;
            return ir_concatenate(2, translate_Exp(exp_node,tp), cnt_ic(cWRITE,1,cnt_op_str(VARIABLE,tp)));
        }else if(pare->node_type == eID&& Exp->children->size == 3){
            char* value = pare->val.idval;

            if(strcmp(value, "read") == 0) {
                return cnt_ic(cREAD,1,cnt_op_str(VARIABLE,place));
            }

            return cnt_ic(CALL,2,cnt_op_str(VARIABLE,place),cnt_op_str(oFUNCTION,value));
        }else if(pare->node_type == eID&& Exp->children->size == 4){
            node* args_node = (node*)pare->pare->children->head->next->next->next->value;

            char* value = pare->val.idval;

            if(strcmp(value, "write") == 0) {
                char* tp = new_place();
                node* third_node = (node*)pare->pare->children->head->next->next->next->value;
                node* exp_node = (node*) third_node->children->head->next->value;
                // print_tree(exp_node, 0);
                return ir_concatenate(2, translate_Exp(exp_node,tp), cnt_ic(cWRITE,1,cnt_op_str(VARIABLE,tp)));
            }

            inter_code* code1 = translate_Arg(args_node);
            return ir_concatenate(2, code1 , cnt_ic(CALL,2,cnt_op_str(VARIABLE, place),cnt_op_str(VARIABLE, value)));
        }
        //condi exp
    int  lb1 = new_lable();
    int  lb2 = new_lable();
    inter_code* code0 = cnt_ic(cASSIGN, 2,cnt_op_str(VARIABLE, place), cnt_op_int(CONSTANT,0));
    inter_code* code1 = translate_cond_Exp(pare, lb1, lb2);
    inter_code* code2 = ir_concatenate(2, cnt_ic(DEF_LAB,1, cnt_op_int(LABEL,lb1)), cnt_ic(cASSIGN, 2,cnt_op_str(VARIABLE, place), cnt_op_int(CONSTANT,1)));
    return ir_concatenate(4,code0 , code1 , code2 ,cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb2)) ) ;

}

//if nothing pare return: LABEL label-4:
inter_code* translate_cond_Exp(node* Exp,int  lb_t,int  lb_f){
    node* pare = (node*)Exp->children->head->next->value;
    if(pare->node_type ==nterm&& strcmp(pare->val.ntermval, "Exp") == 0 && pare->pare->children->size == 3) {
            node* second_node = (node*)pare->pare->children->head->next->next->value;
            node* third_node =  (node*)pare->pare->children->head->next->next->next->value;
            
            if(second_node->node_type == eEQ){
                 
                char* t1= new_place();
                char* t2= new_place();

                 inter_code* code1 = translate_Exp(pare,t1);
                 inter_code* code2 = translate_Exp(third_node,t2);
                 inter_code* code3 =ir_concatenate(2, cnt_ic(cIF,4,cnt_op_str(VARIABLE,t1), rEQ ,cnt_op_str(VARIABLE,t2), cnt_op_int(LABEL,lb_t)),  cnt_ic(GOTO,1,cnt_op_int(LABEL,lb_f) ));
                 return ir_concatenate(3,code1,code2,code3);
            }
            else if(second_node->node_type == eGE){
                 
                char* t1= new_place();
                char* t2= new_place();

                 inter_code* code1 = translate_Exp(pare,t1);
                 inter_code* code2 = translate_Exp(third_node,t2);
                 inter_code* code3 =ir_concatenate(2, cnt_ic(cIF,4,cnt_op_str(VARIABLE,t1), rGE ,cnt_op_str(VARIABLE,t2), cnt_op_int(LABEL,lb_t)),  cnt_ic(GOTO,1,cnt_op_int(LABEL,lb_f) ));
                 return ir_concatenate(3,code1,code2,code3);
            }else if(second_node->node_type == eLE){
                 
                char* t1= new_place();
                char* t2= new_place();

                 inter_code* code1 = translate_Exp(pare,t1);
                 inter_code* code2 = translate_Exp(third_node,t2);
                 inter_code* code3 =ir_concatenate(2, cnt_ic(cIF,4,cnt_op_str(VARIABLE,t1), rLE ,cnt_op_str(VARIABLE,t2), cnt_op_int(LABEL,lb_t)),  cnt_ic(GOTO,1,cnt_op_int(LABEL,lb_f) ));
                 return ir_concatenate(3,code1,code2,code3);
            }
            else if(second_node->node_type == eLT){
                 
                char* t1= new_place();
                char* t2= new_place();

                 inter_code* code1 = translate_Exp(pare,t1);
                 inter_code* code2 = translate_Exp(third_node,t2);
                 inter_code* code3 =ir_concatenate(2, cnt_ic(cIF,4,cnt_op_str(VARIABLE,t1), rLT ,cnt_op_str(VARIABLE,t2), cnt_op_int(LABEL,lb_t)),  cnt_ic(GOTO,1,cnt_op_int(LABEL,lb_f) ));
                 return ir_concatenate(3,code1,code2,code3);
            }
            else if(second_node->node_type == eGT){
                 
                char* t1= new_place();
                char* t2= new_place();

                 inter_code* code1 = translate_Exp(pare,t1);
                 inter_code* code2 = translate_Exp(third_node,t2);
                 inter_code* code3 =ir_concatenate(2, cnt_ic(cIF,4,cnt_op_str(VARIABLE,t1), rGT ,cnt_op_str(VARIABLE,t2), cnt_op_int(LABEL,lb_t)),  cnt_ic(GOTO,1,cnt_op_int(LABEL,lb_f) ));
                 return ir_concatenate(3,code1,code2,code3);
            }else if(second_node->node_type == eNE){
                 
                char* t1= new_place();
                char* t2= new_place();

                 inter_code* code1 = translate_Exp(pare,t1);
                 inter_code* code2 = translate_Exp(third_node,t2);
                 inter_code* code3 =ir_concatenate(2, cnt_ic(cIF,4,cnt_op_str(VARIABLE,t1), rNE ,cnt_op_str(VARIABLE,t2), cnt_op_int(LABEL,lb_t)),  cnt_ic(GOTO,1,cnt_op_int(LABEL,lb_f) ));
                 return ir_concatenate(3,code1,code2,code3);
            }
            

            else if(second_node->node_type == eAND){
            int  lb1= new_lable();
                inter_code* code1 = ir_concatenate(2, translate_cond_Exp(pare,lb1,lb_f), cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb1)));
                inter_code* code2 = translate_cond_Exp(pare,lb_t,lb_f);
                return ir_concatenate(2,code1,code2);
            } 
            else if(second_node->node_type == eOR){
            int  lb1= new_lable();
                inter_code* code1 = ir_concatenate(2, translate_cond_Exp(pare,lb_t,lb1), cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb1)));
                inter_code* code2 = translate_cond_Exp(third_node   ,lb_t,lb_f);
                return ir_concatenate(2,code1,code2);
            }


    }else if(pare->node_type == eNOT) {
        return translate_cond_Exp(Exp,lb_f,lb_t);
    }

    return cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,-4));
}

//if nothing pare return: LABEL label-3:
inter_code* translate_Stmt(node* Stmt){
    Stmt->istranslated = 1;
    node* pare = (node*)Stmt->children->head->next->value;
    if(pare->node_type ==nterm && strcmp(pare->val.ntermval, "Exp") == 0){
        char* tp = new_place();
        return translate_Exp(pare,tp);
    }
    else if(Stmt->children->size== 1){
        node* deflist_pare = (node*)pare->children->head->next->next->value;
        node* stmtlist_pare = (node*)pare->children->head->next->next->next->value;
        // node* stmt_pare = (node*)stmtlist_pare->children->head->next->value;
        // node* next_stmtlist_pare = (node*)stmtlist_pare->children->head->next->next->value;

        inter_code* code1 = translate_DefList(deflist_pare);
        inter_code* code2 = NULL;
        while (stmtlist_pare->isempty == 0)
        {
            node* stmt_pare = (node*)stmtlist_pare->children->head->next->value;
            stmtlist_pare = (node*)stmtlist_pare->children->head->next->next->value;
            code2 =ir_concatenate(2,code2, translate_Stmt(stmt_pare));
        }
        return ir_concatenate(2,code1,code2);
    } 
    if(pare->node_type ==eRETURN){
        node* second_node = (node*)Stmt->children->head->next->next->value;

        char* tp = new_place();
        inter_code* code = translate_Exp(second_node, tp);
        return ir_concatenate(2, code , cnt_ic(cRETURN,1 ,cnt_op_str(VARIABLE,tp)));

    }else if(pare->node_type ==eIF&& pare->pare->children->size <= 6) {
            node* exp_node = (node*)pare->pare->children->head->next->next->next->value;
            node* stmt_node =  (node*)pare->pare->children->head->next->next->next->next->next->value;

        int  lb1 = new_lable();
        int  lb2 = new_lable();
            inter_code* code1 = ir_concatenate(2, translate_cond_Exp(exp_node, lb1, lb2) , cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb1)));
            inter_code* code2 = ir_concatenate(2, translate_Stmt(stmt_node) , cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb2)));
            return ir_concatenate(2, code1 , code2);

    }else if(pare->node_type ==eIF&& pare->pare->children->size > 6) {
            node* exp_node = (node*)pare->pare->children->head->next->next->next->value;
            node* stmt_1_node =  (node*)pare->pare->children->head->next->next->next->next->next->value;
            node* stmt_2_node =  (node*)pare->pare->children->head->next->next->next->next->next->next->next->value;

        int  lb1 = new_lable();
        int  lb2 = new_lable();
        int  lb3 = new_lable();

            inter_code* code1 = ir_concatenate(2, translate_cond_Exp(exp_node, lb1, lb2) , cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb1)));
            inter_code* code2 = ir_concatenate(3, translate_Stmt(stmt_1_node) ,cnt_ic(GOTO,1,cnt_op_int(LABEL,lb3)) , cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb2)));
            inter_code* code3 = ir_concatenate(2, translate_Stmt(stmt_2_node) , cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb3)));

            return ir_concatenate(3, code1 , code2,code3);
    }else if(pare->node_type ==eWHILE) {
            node* exp_node = (node*)pare->pare->children->head->next->next->next->value;
            node* stmt_node =  (node*)pare->pare->children->head->next->next->next->next->next->value;

        int lb1 = new_lable();
        int  lb2 = new_lable();
        int  lb3 = new_lable();

            inter_code* code1 = ir_concatenate(2,cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb1)), translate_cond_Exp(exp_node, lb2, lb3));
            inter_code* code2 = ir_concatenate(3,cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb2)), translate_Stmt(stmt_node),cnt_ic(GOTO,1,cnt_op_int(LABEL,lb1)));

            return ir_concatenate(3, code1 , code2,cnt_ic(DEF_LAB,1,cnt_op_int(LABEL,lb3)));
    }

    return NULL; 
}

inter_code* translate_Arg(node* Arg){
    node* exp_node = (node*)Arg->children->head->next->value;
    char* tp = new_place();
    inter_code* code1 = translate_Exp(exp_node, tp);
    inter_code* code2 = cnt_ic(ARG,1,cnt_op_str(VARIABLE,tp));
    if(Arg->children->size == 3){
        node* args_node = (node*)Arg->children->head->next->next->next->value;
        inter_code* code3 = translate_Arg(args_node);
        return ir_concatenate(3,code3,code1,code2);

    }else{
        return ir_concatenate(3,code1,code2);

    }

}

inter_code* translate_FunDec(node* Fundec){
    node* pare = (node*)Fundec->children->head->next->value;

    if(Fundec->children->size == 3) {
        char* value = pare->val.idval;
        return cnt_ic( DEF_FUNC,1,cnt_op_str(oFUNCTION,value));
    }else{
        char* value = pare->val.idval;
       inter_code* code1 =  cnt_ic( DEF_FUNC,1,cnt_op_str(oFUNCTION,value));

       node* varlist_node = (node*)Fundec->children->head->next->next->next->value;
       inter_code* code2 = translate_VarList(varlist_node);
       return ir_concatenate(2,code1,code2);
    }

}

inter_code* translate_VarList(node* VarList){
     node* param_node = (node*)VarList->children->head->next->value;
     node* var_node = (node*)param_node->children->head->next->next->value;
     while (var_node->children->size > 1)
     {
        var_node = (node*)var_node->children->head->next->value;
     }
     node* id_node = (node*)var_node->children->head->next->value;

    char* value = id_node->val.idval;
    inter_code* code1 = cnt_ic(PARAM,1,cnt_op_str(VARIABLE,value));

    if(VarList->children->size == 3) {
        node* varlist_node = (node*)VarList->children->head->next->next->next->value;
        inter_code* code2 =  translate_VarList(varlist_node);
        return ir_concatenate(2,code1,code2);
    }else {
        return code1;
    }
}
    
inter_code* translate_Dec(node* dec){
    node* vardec_node= (node*)dec->children->head->next->value;
    if (dec->children->size == 1){
        //int j;
        return translate_VarDec(vardec_node);
    }
    else{
        //int j = 7;
        node* vardec_node= (node*)dec->children->head->next->value;
        node* id_node= (node*)vardec_node->children->head->next->value;
        char* value = id_node->val.idval;
        node* exp_node= (node*)dec->children->head->next->next->next->value;
        inter_code* code1 = translate_Exp(exp_node,value);

    }
}

inter_code* translate_VarDec(node* vardec_node){
     int size = 4;
     if (vardec_node->children->size == 1){
         return NULL;
     }
     while (vardec_node->children->size == 4){
         node* varec2_node= (node*)vardec_node->children->head->next->value;
         node* int_node = (node*)vardec_node->children->head->next->next->next->value;
         size *= int_node->val.intval;
         vardec_node = varec2_node;
     }
     node* id_node= (node*)vardec_node->children->head->next->value;
     char* name = id_node->val.idval;
     char* size_str = malloc(sizeof(char)*10);
     sprintf(size_str, "%d", size);
     return cnt_ic(DEC,2,cnt_op_str(VARIABLE,name),cnt_op_str(VARIABLE,size_str));
}

inter_code* translate_StructSpecifier(node* Struct_S){
    MyFieldType* struct_type = Struct_S->type->structure;
    node* id_node = (node*)Struct_S->children->head->next->next->value;
    char* id = id_node->val.idval;
    int size = 0;
    while (struct_type != NULL)
    {
        size += 4;
        struct_type = struct_type->next;   
    }
    char* size_str = malloc(sizeof(char) * 10);
    sprintf(size_str, "%d", size);
    return cnt_ic(DEC , 2 , cnt_op_str(VARIABLE,id),cnt_op_str(VARIABLE,size_str));
    

}

inter_code* translate_exp_addr(node* exp,char* addr, int base){
    node* pare = (node*)exp->children->head->next->value;
    if(pare->node_type == eID&& pare->pare->children->size == 1) {
        char* value = pare->val.idval;
        return cnt_ic(cASSIGN, 2,cnt_op_str(VARIABLE,addr), cnt_op_str(VARIABLE, value));
    }else if(exp->children->size == 4){
            node* exp2_node = (node*)exp->children->head->next->next->next->value;

            char* t1 = new_place();
            char* t2 = new_place();
            char* t3 = new_place();

            inter_code* code1 = translate_Exp(exp2_node,t1);


            //offset
            inter_code* code3 = cnt_ic(cMUL,3,cnt_op_str(VARIABLE,t1),cnt_op_int(CONSTANT,base),cnt_op_str(VARIABLE,t2));
            //addr = exp1_addr + offset
            base = base * pare->type->array->size;
            inter_code* code2 = translate_exp_addr(pare,t3,base);
            inter_code* code4 = cnt_ic(cADD,3,cnt_op_str(VARIABLE,t2),cnt_op_str(VARIABLE,t3),cnt_op_str(VARIABLE,addr));
            //*= not showing
            return  ir_concatenate( 4,code1,code2, code3 , code4);

    }else{
        node* third_node = (node*)exp->children->head->next->next->next->value;

        char* name = third_node->val.idval;
        MyFieldType* field = exp->type->structure;
        int size = 0;
        while (strcmp(field->name,name)!=0)
        {
            size += 4;
            field = field->next;
        }
        char* t1 = new_place();
        inter_code* code1 = translate_exp_addr(pare,t1,4);
        inter_code* code2 = cnt_ic(cADD,3,cnt_op_str(VARIABLE,t1),cnt_op_int(CONSTANT,size),cnt_op_str(VARIABLE,addr));
        return  ir_concatenate( 2,code1,code2);

    }

}

inter_code* translate_DefList(node* DefList){
    inter_code* ir= NULL;
    llist* stack = create_llist(NULL);
    llist_append(stack, create_node(NULL, DefList));
    while (stack->size >= 1)
    {
        node* pare = (node*)llist_pop(stack)->value;
        if (pare->node_type == nterm && strcmp(pare->val.ntermval, "Dec") == 0) {
            inter_code* ir =ir_concatenate(2,ir, translate_Dec(pare));
        }
        // else if (pare->node_type == nterm && strcmp(pare->val.ntermval, "Def") == 0) {

        // }

        if(pare->isempty || pare->children == NULL)   continue;

        llist_node* cur = pare->children->tail->prev;
        while (cur != pare->children->head)
        {
            llist_append(stack, create_node(NULL, cur->value));
            cur = cur->prev;
        }
    } 
    return ir;

}

char* new_place(){
    y += 1;
    char* newplace = malloc(sizeof(char)* 10);
    sprintf(newplace,"t%d", y);
    return newplace;
}

int  new_lable(){
    x += 1;
    return x;
}

inter_code* ir_concatenate(int num,...){

    inter_code** code_array = malloc(sizeof(inter_code*)*num);
    va_list codes;
    va_start(codes,num);
    int j = 0;
    for (int i=0; i<num; i++)
    {
        inter_code* code = va_arg(codes,inter_code*);
        if(code != NULL)
            code_array[j++] = code;
    }
    va_end(codes);

    for(int i=0; i<j-1; i++){
        inter_code* cur = code_array[i];
        while (cur->next != NULL)
            cur = cur->next;
        
        cur->next = code_array[i+1];
        code_array[i+1]->prev = cur;
    }
    return code_array[0];

}
