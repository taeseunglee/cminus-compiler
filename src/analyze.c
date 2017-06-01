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

/* Procedure traverse is a generic recursive 
 * syntax tree traversal routine:
 * it applies preProc in preorder and postProc 
 * in postorder to tree pointed to by t
 */
/*
static void traverse( TreeNode * t,
               void (* preProc) (TreeNode *),
               void (* postProc) (TreeNode *) )
{ if (t != NULL)
  { preProc(t);
    { int i;
      for (i=0; i < MAXCHILDREN; i++)
        traverse(t->child[i],preProc,postProc);
    }
    postProc(t);
    traverse(t->sibling,preProc,postProc);
  }
}
*/
/* nullProc is a do-nothing procedure to 
 * generate preorder-only or postorder-only
 * traversals from traverse
 */
static void nullProc(TreeNode * t)
{
  if (t==NULL) return;
  else return;
}

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

static SymbolInfo * setSymbolInfo (TreeNode *t)
{
  SymbolInfo * symbolInfo;

  if ((symbolInfo = malloc(sizeof(*symbolInfo)) ) == NULL)
    {
      // Error
      DONT_OCCUR_PRINT;
      return NULL;
    }

  switch (t->nodeKind)
    {
      case VariableDeclarationK:
        if (tokenToNodeType(t->attr.varDecl.type_spec->attr.TOK) != IntT)
          return NULL; // TODO: Error Message
        symbolInfo->nodeType = IntT;
        break;


      case ArrayDeclarationK:
        if (tokenToNodeType(t->attr.funcDecl.type_spec->attr.TOK) != IntT)
          return NULL; // TODO: Error message
        symbolInfo->nodeType = IntArrayT; // TODO: Type check
        symbolInfo->attr.arrInfo.len = t->attr.arrDecl._num->attr.NUM; // TODO: bracket type check
        break;


      case FunctionDeclarationK:
        if (tokenToNodeType(t->attr.funcDecl.type_spec->attr.TOK) == ErrorT)
          return NULL; // TODO: Error message
        symbolInfo->nodeType = FuncT;
        symbolInfo->attr.funcInfo.retType =
          tokenToNodeType(t->attr.funcDecl.type_spec->attr.TOK);

        TreeNode * trace;
        NodeType * newParamList;
        int n_param;
        int i;

        n_param = 0;
        trace = t->attr.funcDecl.params;

        // TODO: Parameter == 0 check, Void check
        while (trace)
          {
            ++ n_param;
            trace = trace->sibling;
          }
        if ((newParamList = malloc(n_param * sizeof(NodeType))) == NULL)
          {
            DONT_OCCUR_PRINT;
            return NULL;
          }

        trace = t->attr.funcDecl.params;
        i = 0;
        while (trace)
          {
            if (trace->nodeKind == VariableParameterK)
              newParamList[i] =
                tokenToNodeType(trace->attr.varParam.type_spec->attr.TOK); // Is it need type check?
            else if (trace->nodeKind == ArrayParameterK)
              newParamList[i] =
                tokenToNodeType(trace->attr.arrParam.type_spec->attr.TOK);
            else
              {
                DONT_OCCUR_PRINT; // TODO: check is it really "DON'T occur"
                return NULL;
              }

            ++i;
            trace = trace->sibling;
          }


        symbolInfo->attr.funcInfo.paramTypeList = newParamList;
        break;


      case VariableParameterK:
        if (tokenToNodeType(t->attr.varParam.type_spec->attr.TOK) != IntT)
          return NULL; // TODO: Error Message
        symbolInfo->nodeType = IntT;
        break;


      case ArrayParameterK:
        if (tokenToNodeType(t->attr.arrParam.type_spec->attr.TOK) != IntT)
          return NULL; // TODO: Error Message
        symbolInfo->nodeType = IntArrayT;
        break;
    }


  return symbolInfo;
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
/* Procedure insertNode inserts 
 * identifiers stored in t into 
 * the symbol table 
 */

#define AlreadyPushedScope 2

static void registerSymbol(TreeNode *reg_node, TreeNode *idNode)
{
  int is_cur_scope;
  if (st_lookup(idNode->attr.ID, &is_cur_scope) == -1 || !is_cur_scope)
    {
      /* not yet in table, so treat as new definition */
      st_insert(idNode->attr.ID, idNode->lineno, reg_node, 0 /* TODO: in project 4 */);
    }
  else /* redeclaration */
    {
      printError(idNode, "Declaration", "Redeclaration of symbol \"%s\"", idNode->attr.ID);
    }
}

static void referSymbol(TreeNode *reg_node, TreeNode *idNode)
{
  int is_cur_scope;
  if (st_lookup(idNode->attr.ID, &is_cur_scope) == -1) /* undeclared V/P/F */
    {
      printError(idNode, "Declaration", "Undeclared symbol \"%s\"", idNode->attr.ID);
    }
  else
    {
      /* already in table, so ignore location,
       * add line number of use only */
      st_insert(idNode->attr.ID, idNode->lineno, NULL, 0);
    }
}

static void insertNode( TreeNode * t, int flags)
{ 
  /*
  if (t == NULL)
    {
      printf("[Debug in insertNode] t == NULL!!!\n");
      return;
    }
  */
  for (; t; t = t->sibling)
    {
      switch (t->nodeKind)
        {
          /* Declaration Kinds */
        case VariableDeclarationK:
          registerSymbol(t, t->attr.varDecl._id);
          break;
        case ArrayDeclarationK:
          registerSymbol(t, t->attr.arrDecl._id);
          break;
        case FunctionDeclarationK:
          registerSymbol(t, t->attr.funcDecl._id);
          st_push_scope();
          insertNode(t->attr.funcDecl.params, 0);
          insertNode(t->attr.funcDecl.cmpd_stmt, AlreadyPushedScope);
          break;

          /* Parameter Kinds */
        case VariableParameterK:
          registerSymbol(t, t->attr.varParam._id);
          break;
        case ArrayParameterK:
          registerSymbol(t, t->attr.arrParam._id);
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
          referSymbol(t, t);
          break;
        case ArrayK:
          referSymbol(t, t->attr.arr._id);
          insertNode(t->attr.arr.arr_expr, 0);
          break;
        case CallK:
          referSymbol(t, t->attr.call._id);
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

/* Function buildSymtab constructs the symbol 
 * table by preorder traversal of the syntax tree
 */
void buildSymtab(TreeNode * syntaxTree)
{ //traverse(syntaxTree,insertNode,nullProc);
  insertNode(syntaxTree, 0);
  printSymTab(listing);
  /*
  if (TraceAnalyze)
  { fprintf(listing,"\nSymbol table:\n\n");
    printSymTab(listing);
  }
  */
}


/* Procedure checkNode performs
 * type checking at a single tree node
 */
/*
static void checkNode(TreeNode * t)
{ switch (t->nodekind)
  { case ExpK:
      switch (t->kind.exp)
      { case OpK:
          if ((t->child[0]->type != Integer) ||
              (t->child[1]->type != Integer))
            typeError(t,"Op applied to non-integer");
          if ((t->attr.op == EQ) || (t->attr.op == LT))
            t->type = Boolean;
          else
            t->type = Integer;
          break;
        case ConstK:
        case IdK:
          t->type = Integer;
          break;
        default:
          break;
      }
      break;
    case StmtK:
      switch (t->kind.stmt)
      { case IfK:
          if (t->child[0]->type == Integer)
            typeError(t->child[0],"if test is not Boolean");
          break;
        case AssignK:
          if (t->child[0]->type != Integer)
            typeError(t->child[0],"assignment of non-integer value");
          break;
        case WriteK:
          if (t->child[0]->type != Integer)
            typeError(t->child[0],"write of non-integer value");
          break;
        case RepeatK:
          if (t->child[1]->type == Integer)
            typeError(t->child[1],"repeat test is not Boolean");
          break;
        default:
          break;
      }
      break;
    default:
      break;

  }
}
*/
/* Procedure typeCheck performs type evaluation
 * by syntax tree traversal
 * This procedure does not check whether type of
 * child node is normal.
 */
NodeType typeCheck(TreeNode *t)
{
  if (t == NULL) return NoneT;
  if (t->nodeType != NotResolvedT) return t->nodeType;

  switch (t->nodeKind)
    {
    case VariableDeclarationK:
      if(typeCheck(t->attr.varDecl.type_spec) != IntT)
        {
          printError(t, "Type", "Variable type other than \'int\' is not allowed.");
          t->nodeType = ErrorT;
        }
      else
        {
          t->nodeType = NoneT;
        }
      break;

    case ArrayDeclarationK:
      if(typeCheck(t->attr.arrDecl.type_spec) != IntT)
        {
          printError(t, "Type", "Array type other than \'int\' is not allowed.");
          t->nodeType = ErrorT;
        }
      else
        {
          t->nodeType = NoneT;
        }
      break;

    case FunctionDeclarationK:
      // TODO: Check type of return value
      t->nodeType = NoneT;
      typeCheck(t->attr.funcDecl.cmpd_stmt);
      break;

    case VariableParameterK:
    case ArrayParameterK:
      t->nodeType = NoneT;
      break;

    // order of ExpressionStatementK has been changed for brevity
    case ExpressionStatementK:
      // if expr is NULL (t is ';'), typeEval will return NoneT
      t->nodeType = typeCheck(t->attr.exprStmt.expr);
      break;

    case CompoundStatementK:
      typeCheck(t->attr.cmpdStmt.local_decl);
      typeCheck(t->attr.cmpdStmt.stmt_list);
      break;

    case SelectionStatementK:
      if (typeCheck(t->attr.selectStmt.expr) != IntT)
        {
          // error
          // TODO: error print
          t->nodeType = ErrorT;
        }
      else
        {
          typeCheck(t->attr.selectStmt.if_stmt);
          typeCheck(t->attr.selectStmt.else_stmt);
        }
      break;

    case IterationStatementK:
      if (typeCheck(t->attr.iterStmt.expr) != IntT)
        {
          // error
          // TODO: error print
          t->nodeType = ErrorT;
        }
      else
        {
          typeCheck(t->attr.iterStmt.loop_stmt);
        }
      break;

    case ReturnStatementK:
      typeCheck(t->attr.retStmt.expr);
      t->nodeType = NoneT;
      break;

      // TODO: check for each expression
    case AssignExpressionK:
    {
      NodeType exprType = typeCheck(t->attr.assignStmt.expr);
      NodeType varType = typeCheck(t->attr.assignStmt._var);
      if (varType != IntT || varType != exprType)
        {
          // error
          // TODO: error print
          t->nodeType = ErrorT;
        }
      else
        {
          // TODO: review
          t->nodeType = varType;
        }
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
          // error
          // TODO: error print
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
          // error
          // TODO: error print
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
          // error
          // TODO: error print
          t->nodeType = ErrorT;
        }
    }
      break;

    case ArrayK:
      if (typeCheck(t->attr.arr._id) != IntArrayT)
        {
          // error
          // TODO: error print
          t->nodeType = ErrorT;
        }
      else
        {
          t->nodeType = IntT;
        }
      typeCheck(t->attr.arr.arr_expr);
    case CallK:
    {
      NodeType fType = typeCheck(t->attr.call._id);

      if (fType != FuncT)
        {
          // error
          // TODO: error print
          t->nodeType = ErrorT;
        }
      else
        {
          /* TODO: how we can get function return type? */
          t->nodeType = ErrorT;
        }
      typeCheck(t->attr.call.expr_list);
    }
      break;
      
    case VariableK:
      /* TODO: get variable's type from st_lookup.
       * we have to modify st_lookup for getting variable's type.
       * */
      t->nodeType = ErrorT;
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

  // Recursion
  typeCheck(t->sibling);
}
