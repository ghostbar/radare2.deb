#include <r_util.h>

ut8 *buf[] = { "eax", "ebx", "ecx", NULL };

int main()
{
	struct r_mem_pool_t *pool = r_mem_pool_new(128, 0, 0);
	void *foo = r_mem_pool_alloc(pool);
	foo = r_mem_pool_alloc(pool);

	printf ("%d\n", r_mem_count(buf));

	r_mem_pool_free(pool);
}
