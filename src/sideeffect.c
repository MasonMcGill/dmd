
// Compiler implementation of the D programming language
// Copyright (c) 1999-2012 by Digital Mars
// All Rights Reserved
// written by Walter Bright
// http://www.digitalmars.com
// License for redistribution is by either the Artistic License
// in artistic.txt, or the GNU General Public License in gnu.txt.
// See the included readme.txt for details.

#include <stdio.h>
#include <assert.h>

#include "mars.h"
#include "init.h"
#include "expression.h"
#include "template.h"
#include "statement.h"
#include "mtype.h"
#include "utf.h"
#include "declaration.h"
#include "aggregate.h"
#include "scope.h"
#include "attrib.h"

bool walkPostorder(Expression *e, StoppableVisitor *v);
bool lambdaHasSideEffect(Expression *e);

/********************************************
 * Determine if Expression has any side effects.
 */

bool hasSideEffect(Expression *e)
{
    class LambdaHasSideEffect : public StoppableVisitor
    {
    public:
        LambdaHasSideEffect() {}

        void visit(Expression *e)
        {
            // stop walking if we determine this expression has side effects
            stop = lambdaHasSideEffect(e);
        }
    };

    LambdaHasSideEffect v;
    return walkPostorder(e, &v);
}

bool lambdaHasSideEffect(Expression *e)
{
    switch (e->op)
    {
        // Sort the cases by most frequently used first
        case TOKassign:
        case TOKplusplus:
        case TOKminusminus:
        case TOKdeclaration:
        case TOKconstruct:
        case TOKblit:
        case TOKaddass:
        case TOKminass:
        case TOKcatass:
        case TOKmulass:
        case TOKdivass:
        case TOKmodass:
        case TOKshlass:
        case TOKshrass:
        case TOKushrass:
        case TOKandass:
        case TOKorass:
        case TOKxorass:
        case TOKpowass:
        case TOKin:
        case TOKremove:
        case TOKassert:
        case TOKhalt:
        case TOKdelete:
        case TOKnew:
        case TOKnewanonclass:
            return true;

        case TOKcall:
        {
            CallExp *ce = (CallExp *)e;
            /* Calling a function or delegate that is pure nothrow
             * has no side effects.
             */
            if (ce->e1->type)
            {
                Type *t = ce->e1->type->toBasetype();
                if (t->ty == Tdelegate)
                    t = ((TypeDelegate *)t)->next;
                if (t->ty == Tfunction)
                    ((TypeFunction *)t)->purityLevel();
                if (t->ty == Tfunction && ((TypeFunction *)t)->purity > PUREweak &&
                                          ((TypeFunction *)t)->isnothrow)
                {
                }
                else
                    return true;
            }
            break;
        }

        case TOKcast:
        {
            CastExp *ce = (CastExp *)e;
            /* if:
             *  cast(classtype)func()  // because it may throw
             */
            if (ce->to->ty == Tclass && ce->e1->op == TOKcall && ce->e1->type->ty == Tclass)
                return true;
            break;
        }

        default:
            break;
    }
    return false;
}


/***********************************
 * The result of this expression will be discarded.
 * Complain if the operation has no side effects (and hence is meaningless).
 */
void discardValue(Expression *e)
{
    if (lambdaHasSideEffect(e))     // check side-effect shallowly
        return;
    switch (e->op)
    {
        case TOKcast:
        {
            CastExp *ce = (CastExp *)e;
            if (ce->to->equals(Type::tvoid))
            {
                /*
                 * Don't complain about an expression with no effect if it was cast to void
                 */
                return;
            }
            break;          // complain
        }

        case TOKerror:
            return;

        case TOKcall:
            /* Issue 3882: */
            if (global.params.warnings && !global.gag)
            {
                CallExp *ce = (CallExp *)e;
                if (e->type->ty == Tvoid)
                {
                    /* Don't complain about calling void-returning functions with no side-effect,
                     * because purity and nothrow are inferred, and because some of the
                     * runtime library depends on it. Needs more investigation.
                     *
                     * One possible solution is to restrict this message to only be called in hierarchies that
                     * never call assert (and or not called from inside unittest blocks)
                     */
                }
                else if (ce->e1->type)
                {
                    Type *t = ce->e1->type->toBasetype();
                    if (t->ty == Tdelegate)
                        t = ((TypeDelegate *)t)->next;
                    if (t->ty == Tfunction)
                        ((TypeFunction *)t)->purityLevel();
                    if (t->ty == Tfunction && ((TypeFunction *)t)->purity > PUREweak &&
                                              ((TypeFunction *)t)->isnothrow)
                    {
                        const char *s;
                        if (ce->f)
                            s = ce->f->toPrettyChars();
                        else if (ce->e1->op == TOKstar)
                        {
                            // print 'fp' if ce->e1 is (*fp)
                            s = ((PtrExp *)ce->e1)->e1->toChars();
                        }
                        else
                            s = ce->e1->toChars();

                        e->warning("calling %s without side effects discards return value of type %s, prepend a cast(void) if intentional",
                                   s, e->type->toChars());
                    }
                }
            }
            return;

        case TOKimport:
            e->error("%s has no effect", e->toChars());
            return;

        case TOKandand:
        {
            AndAndExp *aae = (AndAndExp *)e;
            discardValue(aae->e2);
            return;
        }

        case TOKoror:
        {
            OrOrExp *ooe = (OrOrExp *)e;
            discardValue(ooe->e2);
            return;
        }

        case TOKquestion:
        {
            CondExp *ce = (CondExp *)e;
            discardValue(ce->e1);
            discardValue(ce->e2);
            return;
        }

        case TOKcomma:
        {
            CommaExp *ce = (CommaExp *)e;
            /* Check for compiler-generated code of the form  auto __tmp, e, __tmp;
             * In such cases, only check e for side effect (it's OK for __tmp to have
             * no side effect).
             * See Bugzilla 4231 for discussion
             */
            CommaExp* firstComma = ce;
            while (firstComma->e1->op == TOKcomma)
                firstComma = (CommaExp *)firstComma->e1;
            if (firstComma->e1->op == TOKdeclaration &&
                ce->e2->op == TOKvar &&
                ((DeclarationExp *)firstComma->e1)->declaration == ((VarExp*)ce->e2)->var)
            {
                return;
            }
            // Don't check e1 until we cast(void) the a,b code generation
            //discardValue(ce->e1);
            discardValue(ce->e2);
            return;
        }

        case TOKtuple:
            /* Pass without complaint if any of the tuple elements have side effects.
             * Ideally any tuple elements with no side effects should raise an error,
             * this needs more investigation as to what is the right thing to do.
             */
            if (!hasSideEffect(e))
                break;
            return;

        default:
            break;
    }
    e->error("%s has no effect in expression (%s)", Token::toChars(e->op), e->toChars());
}
