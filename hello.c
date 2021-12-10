#include <stdio.h>

double perMonth = 25000;
double total = 0.0;
double willtax = 0.0;
double hastaxed = 0.0;
double gongjijin = 25000 * 0.12 * 2 * 12;

double getTax(double salary)
{
    double kouchu = 17880 * (0.08 + 0.03 + 0.005) + 25000 * 0.12;
    willtax += (salary - kouchu - 5000);
    printf("willtax:%f", willtax);
    double tax = 0;
    if (willtax <= 36000)
    {
        tax = (willtax * 0.03);
    }
    else if (willtax <= 144000)
    {
        tax = (willtax - 36000) * 0.1 + (36000 * 0.03);
    }
    else if (willtax <= 300000)
    {
        tax = (willtax - 144000) * 0.2 + (144000 - 36000) * 0.1 + (36000 * 0.03);
    }
    else if (willtax <= 420000)
    {
        tax = (willtax - 300000) * 0.25 + (300000 - 144000) * 0.2 + (144000 - 36000) * 0.1 + (36000 * 0.03);
    }
    else
    {
        tax = (willtax - 420000) * 0.3 + (420000 - 300000) * 0.25 + (300000 - 144000) * 0.2 + (144000 - 36000) * 0.1 + (36000 * 0.03);
    }
    return tax / 12;
}

int main(int argc, char **argv)
{
    int i;
    i = 1;
    while (i <= 12)
    {
        if (i == 12)
        {
            total += (perMonth+75000);
            double tax = getTax(perMonth+89000);
            printf("tax:%f\n", tax);
            hastaxed += (perMonth - tax);
            break;
        }
        total += perMonth;
        double tax = getTax(perMonth);
        printf("tax:%f\n", tax);
        hastaxed += (perMonth - tax);

        i++;
    }

    // total += 89000;
    // hastaxed += (89000 - getTax(89000));

    printf("=================\nbefore  :  %f\n,willtax:   %f\n,after:   %f\n,gongjijin:  %f\n", total, willtax, hastaxed, gongjijin);
    printf("=================================\n");
    return 0;
}
