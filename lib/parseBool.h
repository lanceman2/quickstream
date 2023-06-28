

static inline bool
qsParseBool(const char *str) {

    if(!*str) return true;

    char c0 = str[0];
    char c1 = str[1];

    if(c0 == 'n' || c0 == 'N' || //no
        c0 == '0' || // 0
        c0 == 'f' || c0 == 'F' || // false
        ((c0 == 'o' || c0 == 'O') &&
        (c1 == 'f' || c1 == 'F')) || // off
        c0 == 'u' || c0 == 'U') // unset
        return false;

    return true;
}
