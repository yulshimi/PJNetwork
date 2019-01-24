#include "utility.h"
uint16_t cksum(void* _data, int len) 
{
    uint8_t* data = (uint8_t*)_data;
    uint16_t sum = 0;
    for(int i=0; i < len; ++i)
    {
        sum += data[i];
    }
    return sum;
}

long timeval_usecdiff(struct timeval* start_time, struct timeval* finish_time)
{
  long usec;
  usec = (finish_time->tv_sec - start_time->tv_sec)*1000000;
  usec += (finish_time->tv_usec- start_time->tv_usec);
  return usec;
}

void calculate_timeout(struct timeval* timeout)
{
    gettimeofday(timeout, NULL);
    timeout->tv_usec += 100000; //Time-out is set to 0.1 sec
    if(1000000 <= timeout->tv_usec)
    {
        timeout->tv_usec -= 1000000;
        timeout->tv_usec += 1;
    }
}

vector<string> splice(string input, char split_char)
{
    string new_str;
    vector<string> ret_vector;
    for(int i=0; i < input.length(); ++i)
    {
        if(input[i] != split_char)
        {
            new_str.push_back(input[i]);
        }
        else
        {
            ret_vector.push_back(new_str);
            new_str.clear();
        }
    }
    ret_vector.push_back(new_str);
    return ret_vector;
}

uint32_t convertTo32Bits(vector<uint32_t> in_vector)
{
    uint32_t ret = 0;
    int index = 0;
    for(int i=3; 0 <= i; --i)
    {
        in_vector[index] = in_vector[index] << 8*i;
        ++index;
    }
    for(int i=0; i < in_vector.size(); ++i)
    {
        ret += in_vector[i];
    }
    return ret;
}

vector<uint32_t> fromStrToUintVector(vector<string> str_vector)
{
    vector<uint32_t> ret_vector;
    for(int i=0; i < str_vector.size(); ++i)
    {
        ret_vector.push_back(stoi(str_vector[i]));
    }
    return ret_vector;
}























