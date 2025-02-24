#ifndef INSTANCE_COUNTER_HPP
#define INSTANCE_COUNTER_HPP

template <typename T>
class InstanceCounter {
private:
    static int count;

protected:
    // Ensure this is accessible by derived classes
    InstanceCounter() { ++count; }
    virtual ~InstanceCounter() { --count; }

public:
    static int getInstanceCount() { return count; }
};

// Define static member
template <typename T>
int InstanceCounter<T>::count = 0;

// Simple "modifier" macro to add instance counting to a class
#define keepcount(class_name) class_name : public InstanceCounter<class_name>

#endif