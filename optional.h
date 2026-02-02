#include <stdexcept>
#include <utility>

// Исключение этого типа должно генерироватся при обращении к пустому optional
class BadOptionalAccess : public std::exception {
public:
    using exception::exception; // Наследуем конструкторы стандартного исключения

    // Переопределяем метод what(), чтобы возвращать понятное сообщение об ошибке
    virtual const char* what() const noexcept override {
        return "Bad optional access";
    }
};

template <typename T>
class Optional {
public:
    // Конструктор по умолчанию: создает пустой объект, is_initialized_ по умолчанию false
    Optional() = default;

    // Конструктор из значения: помечает объект полным и создает T в data_ через placement new
    Optional(const T& value) : is_initialized_{ true }, value_{ new(data_) T(value) } {}

    // Конструктор из временного значения: то же самое, но использует перемещение T(std::move(value))
    Optional(T&& value) : is_initialized_{ true }, value_{ new(data_) T(std::move(value)) } {}

    // Конструктор копирования: если other полон, создаем у себя копию его объекта в data_
    Optional(const Optional& other)
        : is_initialized_{ other.is_initialized_ } {
        if (is_initialized_) {
            value_ = new(data_) T(*other.value_); // Вызываем конструктор копирования типа T
        }
    }

    // Конструктор перемещения: если в other что-то есть, "переносим" это в свою память
    Optional(Optional&& other) noexcept {
        if (other.is_initialized_) {
            value_ = new(data_) T(std::move(*other.value_)); // Перемещаем T из other в this->data_
            is_initialized_ = true; // Теперь у нас есть значение
            // other.is_initialized_ НЕ меняем, чтобы его деструктор позже сам очистил старое место
        }
    }

    // Оператор присваивания значения T:
    Optional& operator=(const T& value) {
        if (is_initialized_) {
            *value_ = value; // Если объект уже создан, просто вызываем его оператор присваивания
        }
        else {
            value_ = new(data_) T(value); // Если мы пустые, создаем объект в памяти data_
            is_initialized_ = true; // Фиксируем наличие значения
        }
        return *this;
    }

    // Оператор перемещающего присваивания значения T:
    Optional& operator=(T&& rhs) noexcept {
        if (is_initialized_) {
            *value_ = std::move(rhs); // Если объект есть, перемещаем в него данные из rhs
        }
        else {
            value_ = new(data_) T(std::move(rhs)); // Если нет — создаем перемещением
            is_initialized_ = true;
        }
        return *this;
    }

    // Оператор копирующего присваивания от другого Optional:
    Optional& operator=(const Optional& rhs) {
        if (this != &rhs) { // Защита от самоприсваивания
            if (is_initialized_ && rhs.is_initialized_) {
                *value_ = *rhs.value_; // Оба полные: просто копируем данные внутри объектов T
            }
            else if (rhs.is_initialized_) {
                value_ = new(data_) T(*rhs.value_); // Мы пустые, rhs полон: создаем копию в себе
                is_initialized_ = true;
            }
            else {
                Reset(); // rhs пуст: мы тоже должны стать пустыми
            }
        }
        return *this;
    }

    // Оператор перемещающего присваивания от другого Optional:
    Optional& operator=(Optional&& rhs) noexcept {
        if (this != &rhs) { // Защита от перемещения в самого себя
            if (rhs.is_initialized_) { // Если у источника есть значение
                if (is_initialized_) {
                    *value_ = std::move(*rhs.value_); // Мы не пустые: перемещаем данные в наш объект T
                }
                else {
                    value_ = new(data_) T(std::move(*rhs.value_)); // Мы пустые: создаем T перемещением
                    is_initialized_ = true;
                }
            }
            else {
                // Если источник пуст, а мы нет — уничтожаем свой объект
                if (is_initialized_) {
                    Reset();
                }
            }
        }
        return *this;
    }

    // Деструктор: если объект был инициализирован, вручную вызываем деструктор типа T
    ~Optional() {
        if (is_initialized_) {
            value_->~T(); // Уничтожаем объект в сырой памяти data_
            is_initialized_ = false;
        }
    }

    // Проверка: содержит ли Optional значение
    bool HasValue() const {
        return is_initialized_;
    }

    // Разыменование: возвращает ссылку на объект (без проверок на пустоту)
    T& operator*() {
        return *value_;
    }

    // Константное разыменование
    const T& operator*() const {
        return *value_;
    }

    // Доступ к членам объекта через стрелку
    T* operator->() {
        return value_;
    }

    // Константный доступ через стрелку
    const T* operator->() const {
        return value_;
    }

    // Безопасный доступ к значению: бросает исключение, если Optional пуст
    T& Value() {
        if (!is_initialized_) {
            throw BadOptionalAccess();
        }
        return *value_;
    }

    // Константный безопасный доступ
    const T& Value() const {
        if (!is_initialized_) {
            throw BadOptionalAccess();
        }
        return *value_;
    }

    // Очистка: переводит Optional в пустое состояние и уничтожает объект T
    void Reset() {
        if (is_initialized_) { // Добавлена проверка на всякий случай
            value_->~T();      // Сначала вызываем деструктор объекта T
            is_initialized_ = false; // Затем сбрасываем флаг
        }
    }

private:
    // data_: массив байт размером с T и выравниванием как у T для хранения объекта
    alignas(T) char data_[sizeof(T)];
    // Флаг: true, если в data_ сейчас сконструирован реальный объект T
    bool is_initialized_ = false;
    // Указатель на начало data_, приведенный к типу T для удобства работы
    T* value_{}; 
};