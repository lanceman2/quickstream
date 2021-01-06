
class Foo {

    public:
        Foo() {  data = 5; };
        ~Foo() { };

    private:
        int data = 0;
};


int main(void) {

    Foo *f = new Foo;

    delete f;
    return 0;
}
