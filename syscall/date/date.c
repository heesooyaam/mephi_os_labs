#include "types.h"
#include "user.h"
#include "date.h"

int main(int argc, char *argv[]) {
    struct rtcdate d;

    if(date(&d) < 0) {
        exit();
    }

    printf(1, "%d-%02d-%02dT%02d:%02d:%02d\n",
           d.year, d.month, d.day, d.hour, d.minute, d.second);

    exit();
}
