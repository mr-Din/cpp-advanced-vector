#pragma once
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {

    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Deallocate(buffer_);
            Swap(rhs);
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // ����������� �������� ����� ������ ������, ��������� �� ��������� ���������
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // �������� ����� ������ ��� n ��������� � ���������� ��������� �� ��
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // ����������� ����� ������, ���������� ����� �� ������ buf ��� ������ Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept 
        : data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0))
    {
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector<T> tmp(rhs);
                Swap(tmp);
            }
            else {
                /* ����������� �������� �� rhs, ������ ��� ������������� �����
                   ��� ������ ������������ */
                std::copy_n(rhs.data_.GetAddress(), std::min(size_, rhs.size_), data_.GetAddress());
                
                if (rhs.size_ < size_) {
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                }
                else {
                    std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        
        CopyOrMoveToUninitialized(new_data);

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        // ���������� �������
        if (new_size < size_) {
            std::destroy_n(data_ + new_size, size_ - new_size);
        }
        // ���������� �������
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() noexcept {
        assert(size_ > 0);
        std::destroy_at(&data_[size_ - 1]);
        --size_;
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return  data_ + size_;
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        
        return *(Emplace(end(), std::forward<Args>(args)...));
    }

    template<typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        
        assert(pos >= begin() && pos <= end());

        if (pos == end() && size_ != data_.Capacity()) {
            iterator res_it = new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
            return res_it;
        }

        size_t index_pos = pos - begin();
        if (size_ != data_.Capacity()) {
            return EmplaceNotRelocation(index_pos, std::forward<Args>(args)...);
        }
        else {
            return EmplaceRelocation(index_pos, std::forward<Args>(args)...);
        }
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos)  noexcept
        (std::is_nothrow_move_assignable_v<T>) {

        assert(pos >= begin() && pos <= end() && size_ > 0);

        size_t index_pos = pos - begin();
        if constexpr (std::is_nothrow_move_constructible_v<T>
            || !std::is_copy_constructible_v<T>) {

            std::move(begin() + index_pos + 1, end(), begin() + index_pos);
        }
        else {
            std::copy(begin() + index_pos + 1, end(), begin() + index_pos);
        }

        std::destroy_at(std::prev(end()));
        --size_;
        return begin() + index_pos;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

private:
    void CopyOrMoveToUninitialized(RawMemory<T>& new_data) {
        if constexpr (std::is_nothrow_move_constructible_v<T>
            || !std::is_copy_constructible_v<T>) {

            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
    }

    template<typename... Args>
    iterator EmplaceNotRelocation(size_t index_pos, Args&&... args) {
        
        new (data_ + size_) T(std::move(*(end() - 1)));
        T tmp(std::forward<Args>(args)...);
        std::move_backward(begin() + index_pos, end() - 1, end());
        *(data_ + index_pos) = std::move(tmp);
        iterator res_it = data_ + index_pos;
        size_++;
        return res_it;
    }

    template<typename... Args>
    iterator EmplaceRelocation(size_t index_pos, Args&&... args) {
        
        size_t new_capacity = (size_ == 0 ? 1 : size_ * 2);
        RawMemory<T> new_data(new_capacity);
        iterator res_it = new (new_data + index_pos) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T>
            || !std::is_copy_constructible_v<T>) {

            std::uninitialized_move(begin(), begin() + index_pos, new_data.GetAddress());
            std::uninitialized_move(begin() + index_pos, end(), res_it + 1);
        }
        else {
            try {
                std::uninitialized_copy(begin(), begin() + index_pos, new_data.GetAddress());
            }
            catch (...) {
                std::destroy_at(res_it);
                throw;
            }
            try {
                std::uninitialized_copy(begin() + index_pos, end(), res_it + 1);
            }
            catch (...) {
                std::destroy_n(new_data.GetAddress(), index_pos + 1);
                throw;
            }
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        size_++;
        return res_it;
    }
    
    RawMemory<T> data_;
    size_t size_ = 0;
};