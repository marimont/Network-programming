enum call_type {
            GET = 0,
            QUIT   = 1
};

enum response_msg {
	OK  = 0,
	ERR = 1
};

union call_msg switch (call_type ctype) {
	case GET:
	    string filename<128>;
	case QUIT:
            void;
};



