Litestore library
=================

About
-----
### What it is and isn't?
Litestore library is a lightweight, embeddable, zero config, key-value style
storage system build on SQLite3 database (http://www.sqlite.org/).

Litestore is quite low level. It can be used as is but is intended to be built
upon. More about the design in section 'Implementation details'.

It's not a full document database or key-value store server system.
LIttle like comparing SQLite to Mysql or PotgreSQL (actually SQLite compares
better than Litestore to Couch...).

### Why should I use it?
* If you just want things to work and don't really care about SQL or playing
with files.
* You want to save multiple values in a consistent transactional
way that regular files just can't do.
* Full blown document databases like, CouchDB, MongoDB or Redis are just too
huge and have too much dependencies.

License
-------
MIT
See LICENSE.txt

SQLite is released in the public domain,
see: http://www.sqlite.org/copyright.html
This library does not modify SQLite, only links against it.

Building
--------
make && sudo make install

### Dependencies
The library:
* C99 compiler (*compile*)

    Should be adaptable to older standard.
    
   Tested with gcc (4.7.3 64bit, Linux)
* Make or your own build system (*compile*)
* SQLite3 library (*compile*, *runtime*)
    
   http://www.sqlite.org/

For tests:
* C++ compiler
* gtest

Versioning
----------
Semantic versioning 2.0: http://semver.org/spec/v2.0.0.html
See file VERSION, for current version.

Usage example
-------------
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "litestore.h"

/* Callback for raw data, that allocates a new string. */
static
int alloc_str(litestore_blob_t value, void* user_data)
{
    char** res = (char**)user_data;
    *res = strndup((const char*)value.data, value.size);
    return LITESTORE_OK;
}

/* Callback for internal errors. */
static
void err(const int error_code, const char* error_str, void* user_data)
{
    printf("ERR: (%d): %s\n", error_code, error_str);
}

int main(const int argc, const char** argv)
{
    if (argc < 2)
    {
        printf("Must provide DB file name!\n");
        return 0;
    }
    else
    {
        printf("Opening DB to '%s'.\n", argv[1]);
    }

    /* Open the connection */
    litestore* ls = NULL;
    litestore_opts opts = {&err, NULL};
    if (litestore_open(argv[1], opts, &ls) != LITESTORE_OK)
    {
        litestore_close(ls);
        printf("ERROR 1!\n");
        return -1;
    }

    /* Save raw data */
    const char* key = "Hello";
    const char* value = "World!";
    if (litestore_save_raw(ls,
                           litestore_slice_str(key),
                           litestore_make_blob(value, strlen(value)))
        != LITESTORE_OK)
    {
        litestore_close(ls);
        printf("ERROR 2!\n");
        return -1;
    }

    /* Read the saved data, note callback usage */
    char* res = NULL;
    if (litestore_get_raw(ls,
                          litestore_slice_str(key),
                          &alloc_str, &res)
        == LITESTORE_OK)
    {
        printf("%s %s\n", key, res);
        free(res);
    }
    else
    {
        printf("ERROR 3!\n");
    }

    /* Clean up, close the connection */
    litestore_close(ls);
    return 0;
}
```

Basics
------
Litestore can save **objects**. Each **object** has a **key** and a **value**.
The value can have different types. The key is a user defined **UTF-8 encoded**
string. It can have arbitrary length, though this naturally affects performance
since objects are accessed based on the key.

### Object values
#### Value types
Litestore can save four (4) different types of objects. These are:
* null
* raw
* array
* key-value

##### Null
Simplest is the **null** type. Basically it can be used as **boolean** type
that either exists or it doesn't. It has no real value and only the key is
stored.

##### Raw
The **raw** type is just a binary **blob** and is saved as is.
It can be used for any user defined type that can be saved as bytes,
either directly or by serializing. Format is totally user dependent.

##### Array
**Arrays** are actually **arrays of blobs**. They retain the order of objects.
I.E. when an array is read, the objects will be in the same order as they
apeared in the original array that was saved.

The array indexes, objects, are blobs and hence have the same propertied as
raw data.

##### Key-value
**Key-value** object is basically a heterogenus **blob-blob map**, where both
the map-key and the map-value are user defined data. One thing to note is that
the map-key is a blob and equality is compared as byte data
(SQLite [implementation](http://www.sqlite.org/datatype3.html)).

Implementation details
----------------------
### Design goals
#### Consistency
The API aims to be concise. All functions return values indicate the success 
status of the function and possible output parameters are given as the right 
most function parameters. All API functions and types are prefixed with 
**litestore_**. The implementation is not leaked to the user.

#### Minimum memory allocations
The only memory Litestore library itself allocates, is during open, for the
**litestore** context object. The underlying implementation (*SQLite3*) does 
allocations on it's own and these can't be avoided. How ever all the data is 
passed as directly as possible and possible allocations are left for the 
user. This enables the usage of custom allocators.

This is also the reasoning behind the callback style API, that admittedly is
more cumbersome to use.

On some systems the length of a string is know after constuction 
(C++ for instance). This is the reason for using the **slice_t** type for
strings. SQLite needs to know the length of the string (or blob) and it is 
more efficient if explicit **strlen()** calls can be avoided.

### Transactions
Litestore can be used with **explicit** transactions or **implicit** 
transactions. Explicit transactions mean that the user calls the **_tx** 
functions explicitely and creates an explicit transaction scope.

If no explicit transaction is created, one will be created on each API call.
And commited if the call succeeds.

For better **performance**, always use **explicit** transactions to group
API calls.

### Thread safety
The API is **not** thread safe. I.E. using the same connection/context in 
multiple threads is not safe. How ever all the state is stored in the context 
so it should be safe to create multiple connections, one for each thread, 
to the same DB. 
SQLite it self is thread safe, if configured properly.
See: http://www.sqlite.org/threadsafe.html

Donate
------

### PayPal
**markku.linnoskivi@gmail.com**
<div>
<form action="https://www.paypal.com/cgi-bin/webscr" method="post" target="_top">
<input type="hidden" name="cmd" value="_s-xclick">
<input type="hidden" name="encrypted" value="-----BEGIN PKCS7-----MIIHTwYJKoZIhvcNAQcEoIIHQDCCBzwCAQExggEwMIIBLAIBADCBlDCBjjELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MRQwEgYDVQQKEwtQYXlQYWwgSW5jLjETMBEGA1UECxQKbGl2ZV9jZXJ0czERMA8GA1UEAxQIbGl2ZV9hcGkxHDAaBgkqhkiG9w0BCQEWDXJlQHBheXBhbC5jb20CAQAwDQYJKoZIhvcNAQEBBQAEgYB8A+NPzdgLAKdw/9FFKGuzu9VN74GPQFtywyYCzkWnZHEGHM80RAetNtChFCvu5Sw/uWthjgnf85nheVVJxOusQwqJdzf0ogBmHLf4PxMUW8sHHdTXsuLnW6M8TjWtBlxGvpAoPXD5a/0NwnSF5rENeJ79fD1IBf3WMhIXpwt33zELMAkGBSsOAwIaBQAwgcwGCSqGSIb3DQEHATAUBggqhkiG9w0DBwQIuwPL/w6d+02Agah30+jA8MohPrNURje8v7xaLXrgrhdZ13axPvCHbioL3zz9sUZf9ZNn5oyEn7L8sD7No2VWnE0LxaQ98B0c+OWikXruPl9QlMSBoVp7IEKEXyH7Cf6lJRRLpGU/q0z2swTP92w3UYOi8TwKQ+bLSU05wCqFN3Sgxb8IdDFLnvcQ965r5BPaq7Spam/JQ4pTS/psAep25o6MYu0PGNvks+Epb7igBhY9+ymgggOHMIIDgzCCAuygAwIBAgIBADANBgkqhkiG9w0BAQUFADCBjjELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MRQwEgYDVQQKEwtQYXlQYWwgSW5jLjETMBEGA1UECxQKbGl2ZV9jZXJ0czERMA8GA1UEAxQIbGl2ZV9hcGkxHDAaBgkqhkiG9w0BCQEWDXJlQHBheXBhbC5jb20wHhcNMDQwMjEzMTAxMzE1WhcNMzUwMjEzMTAxMzE1WjCBjjELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAkNBMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MRQwEgYDVQQKEwtQYXlQYWwgSW5jLjETMBEGA1UECxQKbGl2ZV9jZXJ0czERMA8GA1UEAxQIbGl2ZV9hcGkxHDAaBgkqhkiG9w0BCQEWDXJlQHBheXBhbC5jb20wgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAMFHTt38RMxLXJyO2SmS+Ndl72T7oKJ4u4uw+6awntALWh03PewmIJuzbALScsTS4sZoS1fKciBGoh11gIfHzylvkdNe/hJl66/RGqrj5rFb08sAABNTzDTiqqNpJeBsYs/c2aiGozptX2RlnBktH+SUNpAajW724Nv2Wvhif6sFAgMBAAGjge4wgeswHQYDVR0OBBYEFJaffLvGbxe9WT9S1wob7BDWZJRrMIG7BgNVHSMEgbMwgbCAFJaffLvGbxe9WT9S1wob7BDWZJRroYGUpIGRMIGOMQswCQYDVQQGEwJVUzELMAkGA1UECBMCQ0ExFjAUBgNVBAcTDU1vdW50YWluIFZpZXcxFDASBgNVBAoTC1BheVBhbCBJbmMuMRMwEQYDVQQLFApsaXZlX2NlcnRzMREwDwYDVQQDFAhsaXZlX2FwaTEcMBoGCSqGSIb3DQEJARYNcmVAcGF5cGFsLmNvbYIBADAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBBQUAA4GBAIFfOlaagFrl71+jq6OKidbWFSE+Q4FqROvdgIONth+8kSK//Y/4ihuE4Ymvzn5ceE3S/iBSQQMjyvb+s2TWbQYDwcp129OPIbD9epdr4tJOUNiSojw7BHwYRiPh58S1xGlFgHFXwrEBb3dgNbMUa+u4qectsMAXpVHnD9wIyfmHMYIBmjCCAZYCAQEwgZQwgY4xCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEWMBQGA1UEBxMNTW91bnRhaW4gVmlldzEUMBIGA1UEChMLUGF5UGFsIEluYy4xEzARBgNVBAsUCmxpdmVfY2VydHMxETAPBgNVBAMUCGxpdmVfYXBpMRwwGgYJKoZIhvcNAQkBFg1yZUBwYXlwYWwuY29tAgEAMAkGBSsOAwIaBQCgXTAYBgkqhkiG9w0BCQMxCwYJKoZIhvcNAQcBMBwGCSqGSIb3DQEJBTEPFw0xNDAxMjQwOTU2NTlaMCMGCSqGSIb3DQEJBDEWBBQjN2f7SUeu88JJUu0WIGVX9NrvyzANBgkqhkiG9w0BAQEFAASBgDbB7huxN7/ZJZnZilp6mCI+MCVmUats5q8n6unagcRWwXEP6fhsnVBKExbP3Jbeffke47Dz0IiLMWwolOcML1ka+7tnEmWwbq3ciTACFRuCp5oZrVJEJZevTWSIIs5/ULrSgnUVyzjP9N8rSr0q9w/aWxYUFh5fjtyQezjdWTTW-----END PKCS7-----
">
<input type="image" src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif" border="0" name="submit" alt="PayPal - The safer, easier way to pay online!">
<img alt="" border="0" src="https://www.paypalobjects.com/en_US/i/scr/pixel.gif" width="1" height="1">
</form>
</div>

### Bitcoin
17LEp96XCiTQwTrFWnaQ7jzJC1YjK5pJci
