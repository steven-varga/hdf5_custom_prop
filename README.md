
#Status: **resolved**

[commit 4b2560fa81f](https://bitbucket.hdfgroup.org/projects/HDFFV/repos/hdf5/commits/4b2560fa81fa76a5795a394e9af3da15aa9dab18)
commit '3664cea5d6f87a7e8e708191f9cea36077424a80':
  Add two missing calls to H5I_dec_ref for new dapl_id
  Community-proposed fix

**no credit given**

#HDF5 custom properties

HDF5 CAPI provide extensive library calls to manipulate property lists however there are some inconsistencies between the implementation and documentation: **Dataset Access Property List** doesn't get copied into the dataset internal structure when creating or opening a dataset -- unike dataset control property list. 
In particular dataset creation `dcpl` and dataset access `dapl` property lists may be passed along to [`hid_t H5Dcreate2( hid_t loc_id, const char *name, hid_t dtype_id, hid_t space_id, hid_t lcpl_id, hid_t dcpl_id, hid_t dapl_id )`][create] and [`hid_t H5Dopen2( hid_t loc_id, const char *name, hid_t dapl_id )`][open]
API calls. The expectation is that the property lists will preserve their state during the lifespan of the returned dataset id, this way providing mechanism to attach side band information to opened dataset id, and later retrieve these properties with provided [`H5Dget_create_plist`][get_dcpl] and [`H5Dget_access_plist`][get_dapl]

## Create Custom Properties and `dapl`/`dcpl`
The registering a custom property increases the level of integration with HDF5 CAPI as well as readability/usability of the system. To give ideas these properties may hold memory caches, custom filters with state information tied to opened datasets and so on. Unfortunately when testing the CAPI for preserving properties across calls we found differences[^1] between how `dcpl` and `dapl` has been treated. While the former data control property list preserves the attached custom property the **data access property does not**.

[^1]: Mark Miller, Steven Varga, [see discussion here](https://forum.hdfgroup.org/t/why-do-filters-have-an-upper-limit-for-cd-nelmts/6251/6)

```c
//prop.c
#define H5CPP_DATASET_CUSTOM_PROP "h5cpp_custom_prop"
#define H5CPP_DATASET_CUSTOM_CLASS "h5cpp_custom_access"

/*minimum implementation of a property*/
struct custom_prop_t {
	custom_prop_t(){  std::cout <<"<" << std::hex << this << " CTOR>\n"; }
	~custom_prop_t(){ std::cout <<"<" << std::hex << this << " DTOR>\n"; }
};

herr_t dapl_close_prop( const char *name, size_t size, void *value){
	std::cout << "<prop close: " << std::hex << value << ">\n";
	return 0;
}
 ... rest of callbacks omitted for brevity ... 

herr_t dapl_register_prop( hid_t clazz ){
	custom_prop_t prop;
	return H5Pregister2( clazz, H5CPP_DATASET_CUSTOM_PROP, sizeof(custom_prop_t), &prop,
			dapl_create_prop, dapl_set_prop, dapl_get_prop, 
			dapl_delete_prop, dapl_copy_prop, dapl_compare_prop, dapl_close_prop );
}

// testing harness for custom properties
int main(){
	...
}
```

## Dataset Access Property list
The following code block shows that instead of storing a copy of passed `dapl` property list, the current implementation creates a default property list, then fills it in with details.
```
//H5Dint.c
hid_t H5D_get_access_plist(const H5D_t *dset) {
	...
    /* Make a copy of the default dataset access property list */
    if(NULL == (old_plist = (H5P_genplist_t *)H5I_object(H5P_LST_DATASET_ACCESS_ID_g)))
        HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, FAIL, "not a property list") 
	...
```
Whereas the dataset creation property list retrieves the stored value from the backing dataset structure:
```c
//H5Dint.c
hid_t H5D_get_create_plist(const H5D_t *dset) {
    H5P_genplist_t      *dcpl_plist;            /* Dataset's DCPL */
	...
    /* Check args */
    if(NULL == (dcpl_plist = (H5P_genplist_t *)H5I_object(dset->shared->dcpl_id)))
        HGOTO_ERROR(H5E_DATASET, H5E_BADTYPE, FAIL, "can't get property list")
	...
```
Further investigation tells that the underlying structure for datsets has only field for `dcpl_id` but not for `dapl_id`:
```c
//H5Dpkg.h
typedef struct H5D_shared_t {
    size_t              fo_count;       /* Reference count */
    hbool_t             closing;        /* Flag to indicate dataset is closing */
    hid_t               type_id;        /* ID for dataset's datatype    */
    H5T_t              *type;           /* Datatype for this dataset     */
    H5S_t              *space;          /* Dataspace of this dataset    */
    hid_t               dcpl_id;        /* Dataset creation property id */
    H5D_dcpl_cache_t    dcpl_cache;     /* Cached DCPL values */
    H5O_layout_t        layout;         /* Data layout                  */
    hbool_t             checked_filters;/* TRUE if dataset passes can_apply check */

    /* Cached dataspace info */
    unsigned            ndims;                        /* The dataset's dataspace rank */
    hsize_t             curr_dims[H5S_MAX_RANK];      /* The curr. size of dataset dimensions */
    hsize_t             curr_power2up[H5S_MAX_RANK];  /* The curr. dim sizes, rounded up to next power of 2 */
    hsize_t             max_dims[H5S_MAX_RANK];       /* The max. size of dataset dimensions */

    /* Buffered/cached information for types of raw data storage*/
    struct {
        H5D_rdcdc_t     contig;         /* Information about contiguous data */
                                        /* (Note that the "contig" cache
                                         * information can be used by a chunked
                                         * dataset in certain circumstances)
                                         */
        H5D_rdcc_t      chunk;          /* Information about chunked data */
    } cache;

    H5D_append_flush_t   append_flush;   /* Append flush property information */
    char                *extfile_prefix; /* expanded external file prefix */
    char                *vds_prefix;     /* expanded vds prefix */
} H5D_shared_t;
```
The missing `dapl_id` filed in `H5D_shared_t` type explains why the `H5D_get_access_plist` doesn't follow suit with `H5D_get_create_plist` instead uses default dataset access property.


# What Modifications Needed 
## Structural Modifications
Internally dataset descriptors are represented an `hid_t` tied main component which is specific to each opened dataset descriptor,
```c
//H5Dpkg.h
struct H5D_t {
    H5O_loc_t           oloc;           /* Object header location       */
    H5G_name_t          path;           /* Group hierarchy path         */
    H5D_shared_t        *shared;        /* cached information from file */
};
```
and a shared component that is tied to the actual dataset. In order to extend the existing mechanism for `dcpl` property list to `dapl` lists 
let's introduce `dapl_id` to the shared structure. Then maintain this property similar to `dcpl`.
```c
//H5Dpkg.h
typedef struct H5D_shared_t {
    ...
	hid_t               dcpl_id;        /* Dataset creation property id */
    hid_t               dapl_id;        /* Dataset access property id */
	...
} H5D_shared_t;
```

## Modification to Property Accessor
Lets see how HDF5 internal call `H5D_get_create_plist(const H5D_t *dset)` retrieves the `dcpl` property, so we can replicate the existing mechanism:
```c
//
hid_t H5D_get_create_plist(const H5D_t *dset) {
    H5P_genplist_t      *dcpl_plist;            /* Dataset's DCPL */
	...
    /* Check args */
    if(NULL == (dcpl_plist = (H5P_genplist_t *)H5I_object(dset->shared->dcpl_id)))
        HGOTO_ERROR(H5E_DATASET, H5E_BADTYPE, FAIL, "can't get property list")
	...
```
As it appears `H5I_object( hid_t prop_liis)` retrieves the internal structure of a given property, and the rest of the calls are `dcpl` specific, not interesting in our case, since `H5D_get_access_plist(const H5D_t *dset)` has its own full implementation.

```c
hid_t H5D_get_access_plist(const H5D_t *dset) {

    H5P_genplist_t      *old_plist;     /* Default DAPL */
    H5P_genplist_t      *new_plist;     /* New DAPL */
    hid_t               new_dapl_id = FAIL;
    hid_t               ret_value = FAIL;

    FUNC_ENTER_NOAPI_NOINIT

    /* Make a copy of the default dataset access property list */
	if(NULL == (old_plist = (H5P_genplist_t *)H5I_object(dset->shared->dapl_id)))
        HGOTO_ERROR(H5E_DATASET, H5E_BADTYPE, FAIL, "can't get property list")
	... rest is already implemented ... 
```

## Dataset OPEN

```c
static herr_t H5D__open_oid(H5D_t *dataset, hid_t dapl_id) {
	...
    /* (Set the 'vl_type' parameter to FALSE since it doesn't matter from here) */
    if(NULL == (dataset->shared = H5D__new(H5P_DATASET_CREATE_DEFAULT, FALSE, FALSE)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, FAIL, "memory allocation failed")
	...
```

## Dataset CREATE
```c
H5D_t *
H5D__create(H5F_t *file, hid_t type_id, const H5S_t *space, hid_t dcpl_id,
    hid_t dapl_id){
	...
	    /* Initialize the shared dataset space */
    if(NULL == (new_dset->shared = H5D__new(dcpl_id, TRUE, has_vl_type)))
        HGOTO_ERROR(H5E_RESOURCE, H5E_NOSPACE, NULL, "memory allocation failed")

	...
}
```

## Two Birds with One Stone
Grep-ing the entire project indeed there are only two references to `H5D__new`, therefor it seems to be a good 
candidate for updating `dapl` related information, similarly to `dcpl`

```cpp
static H5D_shared_t *H5D__new(hid_t dcpl_id, hid_t dapl_id, hbool_t creating, hbool_t vl_type){
...
    /* If we are using the default dataset creation property list, during creation
     * don't bother to copy it, just increment the reference count
     */
    if(!vl_type && creating && dcpl_id == H5P_DATASET_CREATE_DEFAULT) {
        if(H5I_inc_ref(dcpl_id, FALSE) < 0)
            HGOTO_ERROR(H5E_DATASET, H5E_CANTINC, NULL, "can't increment default DCPL ID")
        new_dset->dcpl_id = dcpl_id;
    } /* end if */
    else {
        /* Get the property list */
        if(NULL == (plist = (H5P_genplist_t *)H5I_object(dcpl_id)))
            HGOTO_ERROR(H5E_ARGS, H5E_BADTYPE, NULL, "not a property list")

        new_dset->dcpl_id = H5P_copy_plist(plist, FALSE);
    }}
```




[create]: https://support.hdfgroup.org/HDF5/doc/RM/RM_H5D.html#Dataset-Create2
[open]: https://support.hdfgroup.org/HDF5/doc/RM/RM_H5D.html#Dataset-Open2
[register]: https://support.hdfgroup.org/HDF5/doc/RM/RM_H5P.html#Property-Register2
[get_dcpl]: https://support.hdfgroup.org/HDF5/doc/RM/RM_H5D.html#Dataset-GetCreatePlist
[get_dapl]: https://support.hdfgroup.org/HDF5/doc/RM/RM_H5D.html#Dataset-GetAccessPlist
