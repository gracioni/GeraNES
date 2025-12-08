#pragma once

#include <cstdlib>
#include <vector>

template<typename T>
class CircularBuffer
{

public:

    enum OnFullBehaviour { STOP, REPLACE, GROW };

private:

    size_t m_start;
    size_t m_currentSize;
    std::vector<T> m_data;
    OnFullBehaviour m_onFullBehaviour;

    void grow()
    {
        std::vector<T> temp;
        temp.reserve(m_data.size());
        while(!empty()) temp.push_back(read());

        m_data.resize(m_data.size()*2);

        for(size_t i = 0; i < temp.size(); i++)
            m_data[i] = temp[i];

        m_start = 0;
        m_currentSize = temp.size();
    }

    void replace()
    {
        ++m_start;
        m_start %= m_data.size();
        --m_currentSize;
    }

    void onFullBehaviour()
    {
        switch(m_onFullBehaviour)
        {
        case STOP: break; //do nothing
        case GROW: grow(); break;
        case REPLACE: replace(); break;
        }
    }


public:

    CircularBuffer(size_t numberOfItems = 256, OnFullBehaviour onFullBehaviour = REPLACE) : m_data(numberOfItems), m_onFullBehaviour(onFullBehaviour)
    {
        m_start = m_currentSize = 0;
    }

    void write(const T& value)
    {
        if(full()) onFullBehaviour();

        if(full()) return;

        size_t index = (m_start+m_currentSize)%m_data.size();

        m_data[index] = value;
        ++m_currentSize;
    }

    T& peak()
    {
        return m_data[m_start];
    }

    T& read()
    {
        T& ret = peak();

        if(m_currentSize > 0){
            m_start++;
            m_start %= m_data.size();
            --m_currentSize;
        }

        return ret;
    }

    T& peakBack()
    {
        if(m_currentSize == 0) return m_data[m_start];

        size_t index = m_start+m_currentSize-1;
        index %= m_data.size();

        return m_data[index];
    }

    T& readBack()
    {
        T& ret = peakBack();

        if(m_currentSize > 0){
            m_currentSize--;
        }

        return ret;
    }

    T& peakAt(size_t index)
    {
        index = m_start + index;
        index %= m_data.size();
        return m_data[index];
    }

    bool empty()
    {
        return m_currentSize == 0;
    }

    //current number of elements
    size_t size()
    {
        return m_currentSize;
    }

    bool full()
    {
        return m_currentSize == m_data.size();
    }

    void clear()
    {
        m_currentSize = 0;
    }


};
