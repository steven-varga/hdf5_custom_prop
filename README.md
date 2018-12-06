
#HDF5 custom properties

- custom class is registered with the optional callbacks:

```cpp
	/* --------------  custom prop utils  --------------- */
	::herr_t dapl_register_prop(::hid_t clazz){
		custom_prop_t prop;
		return H5Pregister2( clazz, H5CPP_DATASET_ACCESS_PROP, sizeof(custom_prop_t), &prop,
				dapl_create_prop, dapl_set_prop, dapl_get_prop, dapl_delete_prop, dapl_copy_prop, dapl_compare_prop, dapl_close_prop );
	}

	::hid_t dapl_create_class(  ){
		hid_t clazz = H5Pcreate_class( H5P_DATASET_ACCESS, H5CPP_DATASET_ACCESS_CLASS,
				dapl_class_create_func,nullptr, dapl_class_copy_func,nullptr, dapl_class_close_func,nullptr);
		dapl_register_prop( clazz );
		return clazz;
	}

```

when `static` default property is created, the callbacks are properly called, class and property is registered. So far good -- although this also work with any other methods: inserting a custom prop into `dapl`, registering custom prop with `dapl`.
```cpp
hid_t dapl = H5Pcopy( h5::exp::default_dapl );

h5::exp::custom_prop_t prop;
H5Pset(dapl,H5CPP_DATASET_ACCESS_PROP, &prop );

std::cout << "check prop if set on dapl passed to H5Dopen: "
	<< H5Pexist(dapl, H5CPP_DATASET_ACCESS_PROP ) << "\n\n";

```

In order properties to be useful the property expected present -- by some mechanism this invariant maintained inside HDF5 CAPI calls -- when later retrived. In the exibit I find it contrary:

```cpp
hid_t ds = H5Dopen(fd, "mat", dapl);
hid_t dapl_ds = H5Dget_access_plist( ds );

std::cout << "custom property expected be present in retrieved dapl: "
	<< H5Pexist(dapl_ds, H5CPP_DATASET_ACCESS_PROP ) << "\n";
std::cout <<"\n\n";
```

Now did you wonder if the property `dapl_ds` retrieved from dataset is the same class we passed along to `H5Dopen`:
```cpp
H5Pset(dapl_ds, H5CPP_DATASET_ACCESS_PROP, &prop );
```

The reported error tells a different story:
```
HDF5-DIAG: Error detected in HDF5 (1.10.4) thread 0:
  #000: H5P.c line 685 in H5Pset(): unable to set value in plist
    major: Property lists
    minor: Unable to register new atom
  #001: H5Pint.c line 3043 in H5P_set(): can't operate on plist to set value
    major: Property lists
    minor: Can't operate on object
  #002: H5Pint.c line 2670 in H5P__do_prop(): can't find property in skip list
    major: Property lists
    minor: Object not found
did change persist? : 0
```


