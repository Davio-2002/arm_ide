#include <stdio.h>

int square( int x ) {
     return x * x;
}

int main( void ) {
     int n;
     printf( "Enter a number: " );
     if ( scanf( "%d", &n ) != 1 ) {
          return 1;
     }

     int total = 0;
     for ( int i = 1; i <= n; i++ ) {
          total += square( i );
     }
     printf( "Sum of squares up to %d: %d\n", n, total );
     return 0;
}
