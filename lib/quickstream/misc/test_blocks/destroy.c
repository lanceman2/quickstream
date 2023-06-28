
struct UserData;

#define QS_USER_DATA_TYPE  struct UserData *

#include "../../../../include/quickstream.h"
#include "../../../debug.h"


struct UserData {

    int value;
};





#define TEST_VALUE  (0xFE3458E9)

static
struct UserData userData = { .value = TEST_VALUE };




int declare(void) {

    qsSetUserData(&userData);

    return 0;
}



int destroy(struct UserData *d) {

    // Just need to see some feedback:
    WARN("HELLO");

    // It's nice to know that user data works.
    ASSERT(d->value == TEST_VALUE);

    return 0;
}
