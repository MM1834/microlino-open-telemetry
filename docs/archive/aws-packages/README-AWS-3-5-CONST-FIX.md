# AWS-3.5 PubSubClient const fix

`PubSubClient::connected()` is not declared `const`.

The shared wrapper therefore cannot expose:

```cpp
bool connected() const;
```

The fix changes the wrapper to:

```cpp
bool connected();
```
