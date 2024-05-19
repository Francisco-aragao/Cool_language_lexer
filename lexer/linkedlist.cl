class C inherits IO{
    x: Int;           (* no intializer, default to 0 *)
    y: Bool <- false; (* initialize y to false *)

    getx(): Int {x};
    gety(): Bool {y};

    print(): Object {
        {
            out_int(getx());
            out_string("\n");
            if y then out_string("true\\") else out_string("false") fi;
            out_string("\n");
        }
    };
};

class Main {
    c : C;
    main(): Object{
        {
            c <- new C;
            c.print();
        }
    };
};