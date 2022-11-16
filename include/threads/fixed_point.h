#include <stdint.h>

#define F (1 << 14)     //fixed point 1
#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))

// x and y denote fixed_point numbers in 17.14 format
// n is an integer

int int_to_fp(int n);           //integer to floating point
int fp_to_int_round(int x);     //FP를 int로 전환(반올림)
int fp_to_int(int x);           //FP를 int로 전환(버림)
int add_fp(int x, int y);      
int add_mixed(int fp, int num);
int sub_fp(int x, int y);
int sub_mixed(int fp, int num);
int mult_fp(int x, int y);
int mult_mixed(int fp, int num);
int div_fp(int x, int y);
int div_mixed(int fp, int num);

int int_to_fp(int n){
    return n * F;   
}

int fp_to_int_round(int x){
    return x / F;
}

int fp_to_int(int x){
    if(x >= 0){
        return (x + F / 2) / F;
    }else{
        return (x - F / 2) / F;
    }
}

int add_fp(int x, int y){
    return x + y;
}

int add_mixed(int fp, int num){
    return fp + num * F;
}

int sub_fp(int x, int y){
    return x - y;
}

int sub_mixed(int fp, int num){
    return fp - num * F;
}

int mult_fp(int x, int y){
    return ((int64_t)x) * y / F;
}

int mult_mixed(int fp, int num){
    return fp * num;
}

int div_fp(int x, int y){
    return ((int64_t)x) * F / y;
}

int div_mixed(int fp, int num){
    return fp / num;
}