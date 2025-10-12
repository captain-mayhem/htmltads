#include <docsearch.h>

int docsearch_available() {
	return 0;
}

docsearch_result_list docsearch_execute(int x, const char* query, int* count, const char* indexfile,
	const char* dir, const char* w32_doc_dirs[], int i) {
	return nullptr;
}

int docsearch_get_details(docsearch_result_list lst, int i, docsearch_details* d) {
	return 0;
}

void docsearch_free_list(docsearch_result_list lst) {

}