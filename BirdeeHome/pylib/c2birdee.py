UNKNOWN = "unknown"
CONTROL = "control"
IDENTIFIER = "identifier"

class str_stream:
    def __init__(self, instr: str):
        self.instr=instr
        self.cnt = 0
        self.curtok=None
        self.pos = 1
        self.line = 1
        self.kind=UNKNOWN
        self.ignore_list = set()
        self.ignore_list.add("const")
        self.type_map=dict()
        self.next_tok()

    def check(self,b, msg):
        if not b:
            raise RuntimeError("Error at C-defintion string line:{} , pos: {} - {}".format(self.line,self.pos,msg))

    def _next_char(self):
        if self.cnt + 1>=len(self.instr):
            self.cnt  = len(self.instr)
            return None
        self.cnt +=1
        cur=self.instr[self.cnt]
        if cur == '\n':
            self.pos=1
            self.line+=1
        else:
            self.pos+=1
        return cur
    
    def _cur_char(self):
        return self.instr[self.cnt]

    def _next_tok(self):
        if self.cnt>=len(self.instr):
            self.curtok=None
            self.kind=UNKNOWN
            return
        cur=self._cur_char()
        while cur.isspace():
            if not self._next_char():
                self.curtok=None
                self.kind=UNKNOWN
                return
            cur=self._cur_char()
        if cur in "#();,*":
            self.curtok=cur
            self.kind=CONTROL
            self._next_char()
            return
        elif cur.isalpha() or cur=='_':
            self.curtok=cur
            while self._next_char():
                cur = self._cur_char()
                if not cur.isalpha() and cur != '_' and not cur.isdigit():
                    break
                self.curtok += cur
            self.kind=IDENTIFIER
            return
    
    def next_tok(self):
        self._next_tok()
        while self.curtok in self.ignore_list:
            self._next_tok()

def parse_type(stream: str_stream) -> str:
    stream.check(stream.kind==IDENTIFIER, "Expecting an identifier for type")
    rettype = stream.curtok
    if rettype == "struct":
        stream.next_tok()
        stream.check(stream.kind==IDENTIFIER, "Expecting an identifier for type")
        rettype = stream.curtok
    stream.next_tok()
    if stream.curtok=='*':
        rettype = "pointer ##{}##".format(rettype)
        stream.next_tok()
    return rettype if rettype not in stream.type_map else stream.type_map[rettype]

def parse_c_def(src: str) -> list:
    stream = str_stream(src)
    funcs = []
    while stream.curtok:
        tok = stream.curtok
        if tok == '#':
            stream.next_tok()
            stream.check(stream.kind==IDENTIFIER, "Expecting 'define'")
            stream.check(stream.curtok=="define", "Expecting 'define'")
            
            stream.next_tok()
            stream.check(stream.kind==IDENTIFIER, "Expecting an identifier after 'define'")
            defname = stream.curtok
            stream.next_tok()
            
            if stream.kind==IDENTIFIER and stream.pos==1: #if is define name name2
                stream.type_map[defname] = "{} ##{}##".format(stream.curtok, defname)
                stream.next_tok()
            else:
                stream.ignore_list.add(defname)
        else:
            rettype = parse_type(stream)
            stream.check(stream.kind==IDENTIFIER, "Expecting an identifier for function name")
            funcname = stream.curtok
            stream.next_tok()
            stream.check(stream.curtok=='(', "Expecting an (")
            stream.next_tok()
            args=[]
            while True:
                argty=parse_type(stream)
                if stream.kind==IDENTIFIER:
                    argname = stream.curtok
                    stream.next_tok()
                else:
                    argname = "arg" + str(len(args))
                args.append(argname+ " as " + argty)
                if stream.curtok==',':
                    stream.next_tok()
                else:
                    break
            stream.check(stream.curtok==')', "Expecting an )")
            stream.next_tok()
            stream.check(stream.curtok==';', "Expecting an ;")
            stream.next_tok()
            if rettype =='void':
                rettype = ""
            else:
                rettype = "as " + rettype
            funcs.append("declare func {}({}) {}".format(funcname, ','.join(args), rettype))
    return funcs

     
if __name__ == "__main__":
    import sys
    with open(sys.argv[1]) as f:
        src = f.read()
        for line in parse_c_def(src):
            print(line)