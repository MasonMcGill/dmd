/*
REQUIRED_ARGS: -vgc
TEST_OUTPUT:
---
compilable/nogc_warn.d(30): vgc: 'new' causes gc allocation
compilable/nogc_warn.d(31): vgc: 'new' causes gc allocation
compilable/nogc_warn.d(32): vgc: 'new' causes gc allocation
compilable/nogc_warn.d(33): vgc: 'new' causes gc allocation
compilable/nogc_warn.d(38): vgc: 'delete' requires gc
compilable/nogc_warn.d(43): vgc: 'delete' requires gc
compilable/nogc_warn.d(48): vgc: 'delete' requires gc
compilable/nogc_warn.d(55): vgc: Associative array literals cause gc allocation
compilable/nogc_warn.d(56): vgc: Associative array literals cause gc allocation
compilable/nogc_warn.d(62): vgc: Setting 'length' may cause gc allocation
compilable/nogc_warn.d(65): vgc: Setting 'length' may cause gc allocation
compilable/nogc_warn.d(66): vgc: Setting 'length' may cause gc allocation
compilable/nogc_warn.d(71): vgc: Concatenation may cause gc allocation
compilable/nogc_warn.d(72): vgc: Concatenation may cause gc allocation
compilable/nogc_warn.d(75): vgc: Concatenation may cause gc allocation
compilable/nogc_warn.d(76): vgc: Concatenation may cause gc allocation
compilable/nogc_warn.d(81): vgc: Using closure causes gc allocation
compilable/nogc_warn.d-mixin-110(110): vgc: 'new' causes gc allocation
compilable/nogc_warn.d(116): vgc: Indexing an associative array may cause gc allocation
---
*/

struct Struct {}
void testNew()
{
    auto obj = new Object();
    auto s = new Struct();
    auto s2 = new Struct[5];
    auto i = new int[3];
}

void testDelete(Struct* instance)
{
    delete instance;
}

void testDelete(void* instance)
{
    delete instance;
}

void testDelete(Object instance)
{
    delete instance;
}

enum aaLiteral = ["test" : 0];
void testAA()
{
    int[string] aa;
    aa = ["test" : 0];
    aa = aaLiteral;
}

void testLength()
{
    string s;
    s.length = 100;

    int[] arr;
    arr.length += 1;
    arr.length -= 1;
}

void testConcat(string sin)
{
    sin ~= "test";
    string s2 = sin ~ "test";

    int[] arr;
    arr ~= 1;
    arr ~= arr;
}

void closureHelper2(void delegate() d) {}

void testClosure2()
{
    int a;

    void del1()
    {
        a++;
    }

    closureHelper2(&del1);
}

void testIdup()
{
    int[] x;
    auto x2 = x.idup;
    auto s2 = "test".idup;
}

void testDup()
{
    int[] x;
    auto x2 = x.dup;
    auto s2 = "test".dup;
}

// mixins
void testMixin()
{
    mixin("auto a = new " ~ int.stringof ~ " ();");
}

void testAAIndex()
{
    int[string] aa;
    aa["test"] = 0;
}
