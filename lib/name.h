
extern
char *GetUniqueName(struct QsDictionary *dict, uint32_t count,
        const char *name, const char *prefix);

static inline bool ValidChar(char c) {

    if(c < ' ' ||
            c > '~' ||
            c == '\\' ||
            c == '`' ||
            // ':' is the special char that we use in as a sub-name
            // delimiter in full names for blocks that have parents.
            c == ':')
        return false; // not valid
    return true; // valid
}


static inline bool ValidNameString(const char *s) {

    while(*s && ValidChar(*(s++)));
    return !(*s);
}
