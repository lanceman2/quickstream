
struct QsDictionary;


// This Dictionary class is filled with key/value pairs and then built.
// After it is built no more key/value pairs are Inserted.

// bin/quickstreamGUI needs some of the Dictionary functions so we
// use this QS_EXPORT stuff.
//
#ifndef QS_EXPORT
#  define QS_EXPORT extern
#endif


QS_EXPORT
struct QsDictionary *qsDictionaryCreate(void);


QS_EXPORT
void qsDictionaryDestroy(struct QsDictionary *dict);


// Insert key/value if not present.
//
// If the entry exists, the value is not changed and "idict" is set to the
// node that contains the entry.
// 
// Optional parameter "idict" is a pointer to the dictionary node where
// the value entry was inserted.  "idict" can then be used as a
// sub-dictionary to insert and find with.
//
// Returns 0 on success or 1 if already present and -1 if it is not added
// and have an invalid character.
QS_EXPORT
int qsDictionaryInsert(struct QsDictionary *dict,
        const char *key, const void *value,
        struct QsDictionary **idict);


// If found, cleans up the value calling freeValueOnDestroy if it was set.
//
// Returns 0 if it was found and removed, 1 if not found.
QS_EXPORT
int qsDictionaryRemove(struct QsDictionary *dict, const char *key);


// This is the fast Find() function.
//
// Returns element value for key or 0 if not found.
QS_EXPORT
void *qsDictionaryFind(const struct QsDictionary *dict, const char *key);


// Subtree Searching
//
// This is the another fast Find() function.
//
// Parameter "value", if not 0, is set to the value found.
//
// Returns a struct QsDictionary for key or 0 if not found.
//
// This is used to concatenate a series of finds to get to
// a series of values or just the last leaf value in the series.
//
// For example:
//
//    you need the value at the key string "1:2:foo"
//    and you know there are values at "1:" and "1:2:",
//    so you can get the node (1) at "1:" and then from that
//    node get the node (2) at "2:" that is in node 1, and lastly
//    get the value at "1:2:foo" from node 2.  Like so:
//
//  struct QsDictionary *d = qsDictionaryFindDict(dict, "1:", 0);
//  d = qsDictionaryFindDict(d, "2:", 0);
//  void * value = qsDictionaryFind(d, "foo");
//
//  So in this process we concatenated the search like so: "1:2:foo" by
//  breaking it into 3 searches  "1:", "2:", and "foo" and the crazy thing
//  is, is that this is faster than 1 search method because we do not have
//  to concatenate to build the 1 string we are searching for.
//
QS_EXPORT
struct QsDictionary
*qsDictionaryFindDict(const struct QsDictionary *dict,
        const char *key, void **value);


// For setting up a callback that is called when the entry is removed.
// Set freeValueOnDestroy to 0 to remove the callback.
QS_EXPORT
void qsDictionarySetFreeValueOnDestroy(struct QsDictionary *dict,
        void (*freeValueOnDestroy)(void *));


QS_EXPORT
void qsDictionaryPrintDot(const struct QsDictionary *dict, FILE *file);


// Searches through the whole data structure.
//
// Do not screw with the key memory that is passed to callback().
//
// The callback may remove the entry associated with it, but not other
// entries.
//
// If callback returns non-zero the callback stops being called and the
// call to qsDictionaryForEach() returns.
//
// Searches the entire data structure starting at dict.  Calls callback
// with key and value.
//
// Returns the number of keys and callbacks called.
//
QS_EXPORT
size_t
qsDictionaryForEach(const struct QsDictionary *dict,
        int (*callback) (const char *key, void *value,
            void *userData), void *userData);


// Set the value for the node at "dict".
QS_EXPORT
void qsDictionarySetValue(struct QsDictionary *dict, void *value);


// Get the value for the node at "dict".
QS_EXPORT
void *qsDictionaryGetValue(const struct QsDictionary *dict);


// We are using this in ../bin/qsb_block.c
// so we must export this symbol.
//
// TODO: remove this next line by adding a new function
// that check if parameters exist.  See ../bin/qsb_block.c.
//
__attribute__((visibility("default")))
QS_EXPORT
bool qsDictionaryIsEmpty(const struct QsDictionary *dict);
