#ifndef DOCSEARCH_H
#define DOCSEARCH_H

#include <docsearch_export.h>

DOCSEARCH_EXPORT int docsearch_available();

struct DOCSEARCH_EXPORT docsearch_result_list_t {

};

typedef struct docsearch_result_list_t* docsearch_result_list;

struct DOCSEARCH_EXPORT docsearch_details {
    const char* fname;
    const char* title;
    const char* summary;
};

DOCSEARCH_EXPORT docsearch_result_list docsearch_execute(int x, const char* query, int* count, const char* indexfile,
    const char* dir, const char* w32_doc_dirs[], int i);

DOCSEARCH_EXPORT int docsearch_get_details(docsearch_result_list lst, int i, docsearch_details* d);

DOCSEARCH_EXPORT void docsearch_free_list(docsearch_result_list lst);

#endif
