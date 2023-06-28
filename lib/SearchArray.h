// This could be extended to different types.


// We assume that the array[] has elements in ascending order.
//
// O(log2 N) of operation bisection search
//
// It's a little complex, but not O(N^2)
//
static inline
int FindNearestInArray(int x, int *array, int alen) {

    // Find the closest x.
    //
    // We assume that the array[] array has an ascending order.
    //
    // Let "inc" be the amount (increment er) we travel in the array as we
    // search.  It can be positive or negative and it decreases in
    // magnitude as we search.
    int inc = alen/2;
    // Let i be the index into array[] that we are looking for.
    int i = inc;
    if(array[i] > x)
        inc *= -1;
    int prevX = array[i];
    int imax = alen - 1;

    // O(log2 N) bisection search.
    //
    // I don't know why I did this.  It's like a list of 30 values.
    // A simple linear search would have been fine, but why do something
    // easy when you can make it the hard way.
    //
    while((inc > 0 && array[i] < x) || (inc < 0 && array[i] > x)) {

        prevX = array[i];

        if(inc > 1 || inc < -1)
            // Move less this time.
            inc /= 2;

        if((inc > 0 && i < imax) || (inc < 0 && i > 0))
            // move in the chosen direction
            i += inc;
        else
            // We are at an edge in the array[] array.
            return array[i];

        if(abs(inc) == 1)
            if((prevX > x && x > array[i]) ||
                    (prevX < x && x < array[i]))
                // We trapped the value between to ends.
                break;

        if((inc > 0 && array[i] > x) || (inc < 0 && array[i] < x)) {
            // change direction
            inc *= -1;
        }
    }

    if(abs(prevX - x) < abs(array[i] - x))
        return prevX;

    return array[i];
}

