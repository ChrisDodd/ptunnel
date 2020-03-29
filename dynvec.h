#ifndef _dynvec_h_
#define _dynvec_h_

#include <vector>

/// std::vector variant that auto-resizes on access
template<class T, class _Alloc = std::allocator<T>>
class dynvec : public std::vector<T, _Alloc> {
 public:
    using std::vector<T, _Alloc>::vector;
    typedef typename std::vector<T, _Alloc>::reference reference;
    typedef typename std::vector<T, _Alloc>::const_reference const_reference;
    typedef typename std::vector<T, _Alloc>::size_type size_type;
    typedef typename std::vector<T>::const_iterator const_iterator;
    reference operator[](size_type n) {
        if (n >= this->size()) this->resize(n+1);
        return this->at(n); }
    T operator[](size_type n) const { return T(); }
};

#endif /* _dynvec_h_ */
