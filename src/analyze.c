/****************************************************/
/* File: analyze.c                                  */
/* Semantic analyzer implementation                 */
/* for the TINY compiler                            */
/* Compiler Construction: Principles and Practice   */
/* Kenneth C. Louden                                */
/****************************************************/

#include <stdarg.h>

#include "globals.h"
#include "symtab.h"
#include "analyze.h"
#include "symtab.h"

/* counter for variable memory locations */
static int location = 0;

static NodeType tokenToNodeType (TokenType token)
{
  switch (token)
    {
    case INT:
      return IntT;
    case VOID:
      return VoidT;
    default:
      DONT_OCCUR_PRINT;
      return ErrorT;
    }
}

static void printError(TreeNode * t, const char *error_type, const char *fmt, ...)
{
  va_list args;
  fprintf(listing, "%s error at line %d: ", error_type, t->lineno);

  va_start(args, fmt);
  vfprintf(listing, fmt, args);
  va_end(args);

  fprintf(listing, "\n");
  Error = TRUE;
}

static SymbolInfo * setSymbolInfo (TreeNode *t)
{
  SymbolInfo * symbolInfo;

  if (t == NULL) return NULL;
  MALLOC(symbolInfo, sizeof(*symbolInfo));

  switch (t->nodeKind)
    {
      case VariableDeclarationK:
        if (tokenToNodeType(t->attr.varDecl.type_spec->attr.TOK) != IntT)
          {
            printError(t, "Type", "Variable '%s' cannot be void type.", t->attr.varDecl._var->attr.ID);
            return NULL;
          }
        symbolInfo->nodeType = IntT;
        break;


      case ArrayDeclarationK:
        if (tokenToNodeType(t->attr.funcDecl.type_spec->attr.TOK) != IntT)
          {
            printError(t, "Type", "Array '%s' cannot be void type.", t->attr.varDecl._var->attr.ID);
            return NULL;
          }
        symbolInfo->nodeType = IntArrayT; // TODO: Type check
        symbolInfo->attr.arrInfo.len = t->attr.arrDecl._num->attr.NUM; // TODO: bracket type check
        break;


      case FunctionDeclarationK:
        if (tokenToNodeType(t->attr.funcDecl.type_spec->attr.TOK) == ErrorT)
          {
            DONT_OCCUR_PRINT;
            return NULL;
          }
        symbolInfo->nodeType = FuncT;
        symbolInfo->attr.funcInfo.retType =
          tokenToNodeType(t->attr.funcDecl.type_spec->attr.TOK);

        TreeNode * param_node;
        NodeType * newParamList;
        int n_param;
        int i;

        n_param = 0;
        param_node = t->attr.funcDecl.params;

        while (param_node)
          {
            ++ n_param;
            param_node = param_node->sibling;
          }

        MALLOC(newParamList, n_param * sizeof(NodeType));

        param_node = t->attr.funcDecl.params;
        i = 0;
        while (param_node)
          {
            if (param_node->nodeKind == VariableParameterK)
              newParamList[i] =
                tokenToNodeType(param_node->attr.varParam.type_spec->attr.TOK); // Is it need type check?
            else if (param_node->nodeKind == ArrayParameterK)
              {
                newParamList[i] =
                  tokenToNodeType(param_node->attr.arrParam.type_spec->attr.TOK);
                if (newParamList[i] != IntT)
                  {
                    DONT_OCCUR_PRINT;
                    return NULL;
                  }
                newParamList[i] = IntArrayT;
              }
            else
              {
                DONT_OCCUR_PRINT; // TODO: check is it really "DON'T occur"
                return NULL;
              }

            ++i;
            param_node = param_node->sibling;
          }

        symbolInfo->attr.funcInfo.paramTypeList = newParamList;
        symbolInfo->attr.funcInfo.len = n_param;
        break;


      case VariableParameterK:
        if (tokenToNodeType(t->attr.varParam.type_spec->attr.TOK) != IntT)
          {
            printError(t, "Type", "Variable parameter '%s' cannot be void type.", t->attr.varDecl._var->attr.ID);
            return NULL;
          }
        symbolInfo->nodeType = IntT;
        break;


      case ArrayParameterK:
        if (tokenToNodeType(t->attr.arrParam.type_spec->attr.TOK) != IntT)
          {
            printError(t, "Type", "Array parameter '%s' cannot be void type.", t->attr.varDecl._var->attr.ID);
            return NULL;
          }
        symbolInfo->nodeType = IntArrayT;
        break;

      default:
        DONT_OCCUR_PRINT;
        free(symbolInfo);
        //symbolInfo = NULL;
        break;
    }

  return symbolInfo;
}

#define AlreadyPushedScope 2

static int registerSymbol(TreeNode *regNode, TreeNode *varNode, SymbolInfo * symbolInfo)
{
  int is_cur_scope;
  if (st_lookup(varNode->attr.ID, &is_cur_scope) == NULL || !is_cur_scope)
    {
      /* not yet in table, so treat as new definition */
      st_register(varNode->attr.ID, varNode->lineno, symbolInfo);
      varNode->nodeType = symbolInfo->nodeType;
      varNode->symbolInfo = symbolInfo;
      return TRUE;
    }
  else /* redeclaration */
    {
      printError(varNode, "Declaration", "Redeclaration of symbol \"%s\"", varNode->attr.ID);
      varNode->nodeType = ErrorT;
      return FALSE;
    }
}

static void referSymbol(TreeNode *varNode)
{
  int is_cur_scope;
  SymbolInfo * symbolInfo;
  if ((symbolInfo = st_lookup(varNode->attr.ID, &is_cur_scope)) == NULL) /* undeclared V/P/F */
    {
      printError(varNode, "Declaration", "Undeclared symbol \"%s\"", varNode->attr.ID);
    }
  else
    {
      /* already in table, so ignore location,
       *
       * add line number of use only */
      st_refer(varNode->attr.ID, varNode->lineno);
      varNode->nodeType = symbolInfo->nodeType;
      varNode->symbolInfo = symbolInfo;
    }
}

static void insertNode( TreeNode * t, int flags)
{
  for (; t; t = t->sibling)
    {
      SymbolInfo * symbolInfo = NULL;
      int registerSuccess = 0;

      switch (t->nodeKind)
        {
          /* Declaration Kinds */
        case VariableDeclarationK:
          symbolInfo = setSymbolInfo(t);
          if(symbolInfo)
            registerSuccess = registerSymbol(t, t->attr.varDecl._var, symbolInfo);
          if(!registerSuccess || !symbolInfo) 
            t->nodeType = ErrorT;
          break;
        case ArrayDeclarationK:
          symbolInfo = setSymbolInfo(t);
          if(symbolInfo)
            registerSuccess = registerSymbol(t, t->attr.arrDecl._var, symbolInfo);
          if(!registerSuccess || !symbolInfo) 
            t->nodeType = ErrorT;
          break;
        case FunctionDeclarationK:
          symbolInfo = setSymbolInfo(t);
          if(symbolInfo)
            registerSuccess = registerSymbol(t, t->attr.funcDecl._var, symbolInfo);
          if(!registerSuccess || !symbolInfo)
            {
              t->nodeType = ErrorT;
              //break; // TODO: review
            }
          st_push_scope();
          insertNode(t->attr.funcDecl.params, 0);
          insertNode(t->attr.funcDecl.cmpd_stmt, AlreadyPushedScope);
          break;

          /* Parameter Kinds */
        case VariableParameterK:
          symbolInfo = setSymbolInfo(t);
          if(symbolInfo)
            registerSuccess = registerSymbol(t, t->attr.varParam._var, symbolInfo);
          if(!registerSuccess || !symbolInfo)
            t->nodeType = ErrorT;
          break;
        case ArrayParameterK:
          symbolInfo = setSymbolInfo(t);
          if(symbolInfo)
            registerSuccess = registerSymbol(t, t->attr.arrParam._var, symbolInfo);
          if(!registerSuccess || !symbolInfo)
            t->nodeType = ErrorT;
          break;

          /* Statement Kinds */
        case CompoundStatementK:
          if (!(flags & AlreadyPushedScope))
            st_push_scope();
          insertNode(t->attr.cmpdStmt.local_decl, 0);
          insertNode(t->attr.cmpdStmt.stmt_list, 0);
          printSymTab(listing);
          st_pop_scope();
          break;
        case ExpressionStatementK:
          insertNode(t->attr.exprStmt.expr, 0);
          break;
        case SelectionStatementK:
          insertNode(t->attr.selectStmt.expr, 0);
          insertNode(t->attr.selectStmt.if_stmt, 0);
          insertNode(t->attr.selectStmt.else_stmt, 0);
          break;
        case IterationStatementK:
          insertNode(t->attr.iterStmt.expr, 0);
          insertNode(t->attr.iterStmt.loop_stmt, 0);
          break;
        case ReturnStatementK:
          insertNode(t->attr.retStmt.expr, 0);
          break;

          /* Expression Kinds */
        case AssignExpressionK:
          insertNode(t->attr.assignStmt.expr, 0);
          insertNode(t->attr.assignStmt._var, 0);
          break;
        case ComparisonExpressionK:
          insertNode(t->attr.cmpExpr.lexpr, 0);
          insertNode(t->attr.cmpExpr.op, 0);
          insertNode(t->attr.cmpExpr.rexpr, 0);
          break;
        case AdditiveExpressionK:
          insertNode(t->attr.addExpr.lexpr, 0);
          insertNode(t->attr.addExpr.op, 0);
          insertNode(t->attr.addExpr.rexpr, 0);
          break;
        case MultiplicativeExpressionK:
          insertNode(t->attr.multExpr.lexpr, 0);
          insertNode(t->attr.multExpr.op, 0);
          insertNode(t->attr.multExpr.rexpr, 0);
          break;

        case VariableK:
          referSymbol(t);
          break;
        case ArrayK:
          insertNode(t->attr.arr._var, 0);
          insertNode(t->attr.arr.arr_expr, 0);
          break;
        case CallK:
          insertNode(t->attr.call._var, 0);
          insertNode(t->attr.call.expr_list, 0);
          break;

          /* Leaf Nodes */
          /* Cannot be reached here!! */
        case ConstantK:
          /* nothing to do */
          break;
        case TokenTypeK:
          /* nothing to do */
          break;
        case ErrorK:
          DONT_OCCUR_PRINT;
          break;
        default:
          DONT_OCCUR_PRINT;
        }
    }
}

static void mainCheck(TreeNode *t)
{
  if(t == NULL)
    {
      printError(t, "Main", "There is no main function.");
      return;
    }
  for(; t->sibling; t=t->sibling);
  if(t->nodeKind != FunctionDeclarationK
     || strcmp(t->attr.funcDecl._var->attr.ID, "main") != 0)
    {
      printError(t, "Main", "Main function must be declared at the very last of program.");
      return;
    }

  if(t->attr.funcDecl.type_spec->attr.TOK != VOID)
    {
      printError(t, "Type", "Main function must be type void");
      return;
    }
  if(t->attr.funcDecl.params != NULL)
    {
      printError(t, "Main", "Main function cannot have parameter.");
      return;
    }
}

/* Function buildSymtab constructs the symbol 
 * table by preorder traversal of the syntax tree
 */
void buildSymtab(TreeNode * syntaxTree)
{
  insertNode(syntaxTree, 0);
  printSymTab(listing);
  typeCheck(syntaxTree);
  mainCheck(syntaxTree);
  /*
  if (TraceAnalyze)
  { fprintf(listing,"\nSymbol table:\n\n");
    printSymTab(listing);
  }
  */
}

/* Procedure typeCheck performs type evaluation
 * by syntax tree traversal
 */
NodeType typeCheck(TreeNode *n)
{
  if (n == NULL) return NoneT;
  if (n->nodeType != NotResolvedT) return n->nodeType;

  TreeNode *t = n;
  for(t=n; t; t=t->sibling)
    {
      switch (t->nodeKind)
        {
        case VariableDeclarationK:
          if(t->attr.varDecl.type_spec->attr.TOK != INT)
            {
              printError(t, "Type", "Variable type other than 'int' is not allowed.");
              t->nodeType = ErrorT;
            }
          else
            {
              t->nodeType = NoneT;
            }
          break;

        case ArrayDeclarationK:
          if(t->attr.arrDecl.type_spec->attr.TOK != INT)
            {
              printError(t, "Type", "Array type other than 'int' is not allowed.");
              t->nodeType = ErrorT;
            }
          else
            {
              t->nodeType = NoneT;
            }
          break;

        case FunctionDeclarationK:
        {
          TreeNode *cmpdStmt = t->attr.funcDecl.cmpd_stmt;
          if(cmpdStmt != NULL)
            {
              if(t->attr.arrDecl.type_spec->attr.TOK == INT)
                cmpdStmt->attr.cmpdStmt.retType = IntT;
              else if(t->attr.arrDecl.type_spec->attr.TOK == VOID)
                cmpdStmt->attr.cmpdStmt.retType = VoidT;
              else
                DONT_OCCUR_PRINT;
              typeCheck(t->attr.funcDecl.cmpd_stmt);
            }
          t->nodeType = NoneT;
        }
          break;

        case VariableParameterK:
          if(t->attr.varParam.type_spec->attr.TOK != INT)
            {
              printError(t, "Type", "Parameter type other than 'int' or 'int[ ]' is not allowed.");
              t->nodeType = ErrorT;
            }
          else
            {
              t->nodeType = NoneT;
            }
          break;

        case ArrayParameterK:
          if(t->attr.arrParam.type_spec->attr.TOK != INT)
            {
              printError(t, "Type", "Parameter type other than 'int' or 'int[ ]' is not allowed.");
              t->nodeType = ErrorT;
            }
          else
            {
              t->nodeType = NoneT;
            }
          break;

        case CompoundStatementK:
        {
          // Propagate return type for return statement
          TreeNode *stmt;
          for(stmt = t->attr.cmpdStmt.stmt_list;
              stmt != NULL;
              stmt = stmt->sibling)
            {
              if(stmt->nodeKind == ReturnStatementK)
                {
                  stmt->attr.retStmt.retType = t->attr.cmpdStmt.retType;
                }
              if(stmt->nodeKind == CompoundStatementK)
                {
                  //TODO: need to compare equalness.
                  stmt->attr.cmpdStmt.retType = t->attr.cmpdStmt.retType;
                }
            }

          typeCheck(t->attr.cmpdStmt.local_decl);
          typeCheck(t->attr.cmpdStmt.stmt_list);
          t->nodeType = NoneT;
        }
          break;

        case ExpressionStatementK:
          typeCheck(t->attr.exprStmt.expr);
          t->nodeType = NoneT;
          break;

        case SelectionStatementK:
          if (typeCheck(t->attr.selectStmt.expr) != IntT)
            {
              printError(t, "Type", "Expression inside selection statement should be type 'int'.");
              t->nodeType = ErrorT;
            }
          else
            {
              typeCheck(t->attr.selectStmt.if_stmt);
              typeCheck(t->attr.selectStmt.else_stmt);
              t->nodeType = NoneT;
            }
          break;

        case IterationStatementK:
          if (typeCheck(t->attr.iterStmt.expr) != IntT)
            {
              printError(t, "Type", "Expression inside iteration statement should be type 'int'.");
              t->nodeType = ErrorT;
            }
          else
            {
              typeCheck(t->attr.iterStmt.loop_stmt);
              t->nodeType = NoneT;
            }
          break;

        case ReturnStatementK:
        {
          if(t->attr.retStmt.retType != IntT
             && t->attr.retStmt.retType != VoidT)
            {
              DONT_OCCUR_PRINT;
              t->nodeType = ErrorT;
            }
          else
            {
              NodeType type = typeCheck(t->attr.retStmt.expr);
              if(type != t->attr.retStmt.retType)
                {
                  printError(t, "Type", "Returning expression must match pre-declared function return type.");
                  t->nodeType = ErrorT;
                }
              else
                {
                  t->nodeType = NoneT;
                }
            }
        }
          break;

        case AssignExpressionK:
        {
          NodeType exprType = typeCheck(t->attr.assignStmt.expr);
          NodeType varType = typeCheck(t->attr.assignStmt._var);
          if (varType != IntT || varType != exprType)
            {
              if(varType != ErrorT && exprType != ErrorT)
                printError(t, "Type", "Assignment and assignee type mismatch");
            }
          t->nodeType = varType;
        }
          break;

        case ComparisonExpressionK:
        {
          NodeType lType = typeCheck(t->attr.cmpExpr.lexpr);
          NodeType rType = typeCheck(t->attr.cmpExpr.rexpr);
          if (lType == IntT && lType == rType)
            {
              t->nodeType = IntT;
            }
          else
            {
              printError(t, "Type", "Comparison between different types.");
              t->nodeType = ErrorT;
            }
        }
          break;

        case AdditiveExpressionK:
        {
          NodeType lType = typeCheck(t->attr.addExpr.lexpr);
          NodeType rType = typeCheck(t->attr.addExpr.rexpr);
          if (lType == IntT && lType == rType)
            {
              t->nodeType = IntT;
            }
          else
            {
              printError(t, "Type", "Addition between different types.");
              t->nodeType = ErrorT;
            }
        }
          break;

        case MultiplicativeExpressionK:
        {
          NodeType lType = typeCheck(t->attr.multExpr.lexpr);
          NodeType rType = typeCheck(t->attr.multExpr.rexpr);
          if (lType == IntT && lType == rType)
            {
              t->nodeType = IntT;
            }
          else
            {
              printError(t, "Type", "Multiplication between different types.");
              t->nodeType = ErrorT;
            }
        }
          break;

        case ArrayK:
        {
          int isError = FALSE;
          if (typeCheck(t->attr.arr._var) != IntArrayT)
            {
              printError(t, "Type", "Variable '%s' is not subscriptable.", t->attr.arr._var->attr.ID);
              t->nodeType = ErrorT;
              isError = TRUE;
            }
          if(typeCheck(t->attr.arr.arr_expr) != IntT)
            {
              printError(t, "Type", "Array subscript must be type 'int'.", t->attr.arr._var->attr.ID);
              t->nodeType = ErrorT;
              isError = TRUE;
            }

          if(!isError)
            {
              t->nodeType = IntT;
            }
        }
          break;

        case CallK:
        {
          int isError = FALSE;
          SymbolInfo *info = t->attr.call._var->symbolInfo;
          NodeType fType = typeCheck(t->attr.call._var);

          if (fType != FuncT)
            {
              if (fType == IntT || fType == IntArrayT)
                {
                  printError(t, "Type", "Variable '%s' is not callable.", t->attr.call._var->attr.ID);
                }
              else
                {
                  printError(t, "Type", "'%s' is never declared.", t->attr.call._var->attr.ID);
                }
              t->nodeType = ErrorT;
              isError = TRUE;
            }

          if (!isError && info == NULL)
            {
              t->nodeType = ErrorT;
              isError = TRUE;
              DONT_OCCUR_PRINT;
            }

          if(!isError)
            {
              // Parameter type checking
              int exprIdx = 0;
              TreeNode *expr;
              for(expr = t->attr.call.expr_list;
                  expr != NULL;
                  expr = expr->sibling, exprIdx++)
                {
                  if(exprIdx >= info->attr.funcInfo.len)
                    {
                      printError(t,
                                 "Type",
                                 "Too many parameters while calling function '%s'.",
                                 t->attr.call._var->attr.ID);
                      isError = TRUE;
                      break;
                    }
                  if(typeCheck(expr) != info->attr.funcInfo.paramTypeList[exprIdx])
                    {
                      printError(t,
                                 "Type",
                                 "Type mismatch of parameter at %d while calling '%s'.",
                                 exprIdx + 1,
                                 t->attr.call._var->attr.ID);
                      isError = TRUE;
                    }
                }
              if(exprIdx < info->attr.funcInfo.len)
                {
                  printError(t,
                             "Type",
                             "Too little parameters while calling function '%s'.",
                             t->attr.call._var->attr.ID);
                  isError = TRUE;
                  break;
                }
            }

          if(!isError)
            {
              t->nodeType = info->attr.funcInfo.retType;
            }
        }
          break;

        case VariableK:
          /* TODO: get variable's type from st_lookup.
           * we have to modify st_lookup for getting variable's type.
           * */
          //t->nodeType = ;
          break;

        case ConstantK:
          t->nodeType = IntT;
          break;
        case TokenTypeK:
          t->nodeType = NoneT;
          break;

        case ErrorK:
          DONT_OCCUR_PRINT;
        default:
          DONT_OCCUR_PRINT;
          break;
        }
    }

  return n->nodeType;
}
