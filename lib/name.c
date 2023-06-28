#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "debug.h"
#include "Dictionary.h"
#include "name.h"



// The non-zero success return value must be freed by the user.
//
// If dict is 0 than the name will not be checked with a dictionary.
//
char *GetUniqueName(struct QsDictionary *dict, uint32_t count,
        const char *name, const char *prefix) {

    if(name && !name[0])
        name = 0;

    // Get unique name.
    if(name) {

        if(!ValidNameString(name)) {
            ERROR("Name \"%s\" is invalid", name);
            return 0;
        }
        if(dict && qsDictionaryFind(dict, name)) {
            ERROR("Name \"%s\" already exists", name);
            return 0;
        }
        name = strdup(name);

    } else {
        // name == 0 so generate a name.
        const size_t Len = strlen(prefix) + 12;
        char nname[Len];
        do
            snprintf(nname, Len, "%s%" PRIu32, prefix, count++);
        while(dict && qsDictionaryFind(dict, nname));

        name = strdup(nname);
    }
    ASSERT(name, "strdup() failed");
    return (char *) name;
}
