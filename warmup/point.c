#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "point.h"

/* update *p by increasing p->x by x and p->y by y */
void point_translate(struct point *p, double x, double y)
{
	p -> x += x;
	p -> y += y;
}

/* return the cartesian distance between p1 and p2 */
double point_distance(const struct point *p1, const struct point *p2)
{
	return sqrt(pow((p2 -> x) - (p1 -> x), 2) + pow((p2 -> y) - (p1 -> y), 2));
}

/* this function compares the Euclidean lengths of p1 and p2. The Euclidean
 * length of a point is the distance of the point from the origin (0, 0). The
 * function should return -1, 0, or 1, depending on whether p1 has smaller
 * length, equal length, or larger length, than p2. */
int point_compare(const struct point *p1, const struct point *p2)
{
	float distance_p1 = sqrt(pow(p1 -> x, 2) + pow(p1 -> y, 2));
	float distance_p2 = sqrt(pow(p2 -> x, 2) + pow(p2 -> y, 2));
	if (distance_p1 == distance_p2){
		return 0;
	}else if (distance_p1 < distance_p2){
		return -1;
	}else{
		return 1;
	}
}

