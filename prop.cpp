/* Steven Varga 2019 oct 11
 * PURPOSE: to demonstrate custom property NOT copied to dataset internal dapl
 */

#include <iostream>
#include <hdf5.h>

#define H5CPP_DATASET_CUSTOM_PROP "h5cpp_custom_prop"
#define H5CPP_DATASET_CUSTOM_CLASS "h5cpp_custom_access"

/*minimum implementation of a property*/
struct custom_prop_t {
	custom_prop_t(){  std::cout <<"<" << std::hex << this << " CTOR>\n"; }
	~custom_prop_t(){ std::cout <<"<" << std::hex << this << " DTOR>\n"; }
};

herr_t dapl_class_create_func( hid_t prop_id, void* ptr){
	std::cout <<"<class create>\n";
	return 1;
}
herr_t dapl_class_copy_func( hid_t id_to, hid_t id_from, void* ptr){
	std::cout <<"<class copy>\n";
	return 1;
}
herr_t dapl_class_close_func( hid_t id, void* ptr){
	std::cout << "<class close>\n";
	return 1;
}

herr_t dapl_create_prop( const char *name, size_t size, void *value){
	std::cout << "<prop create " << std::hex << value << ">\n";
	return 0;
}
herr_t dapl_set_prop( hid_t prop_id, const char *name, size_t size, void *value){
	std::cout << "<prop set id="<< prop_id << " " << std::hex << value << ">\n";
	return 0;
}
herr_t dapl_get_prop( hid_t prop_id, const char *name, size_t size, void *value){
	std::cout << "<prop get id="<< prop_id << " " << std::hex << value << ">\n";
	return 0;
}

herr_t dapl_delete_prop( hid_t prop_id, const char *name, size_t size, void *value){
	std::cout << "<prop delete id="<< prop_id << " " << std::hex << value << ">\n";
	return 0;
}
herr_t dapl_copy_prop( const char *name, size_t size, void *value){
	std::cout << "<prop copy: " << std::hex << value << ">\n";
	return 0;
}
int dapl_compare_prop( const void *value1, const void *value2, size_t size){
	std::cout << "<prop compare: " << std::hex << value1 << " " <<  value2<<">\n";
	return 0;
}
herr_t dapl_close_prop( const char *name, size_t size, void *value){
	std::cout << "<prop close: " << std::hex << value << ">\n";
	return 0;
}

herr_t dapl_register_prop( hid_t clazz ){
	custom_prop_t prop;
	return H5Pregister2( clazz, H5CPP_DATASET_CUSTOM_PROP, sizeof(custom_prop_t), &prop,
			dapl_create_prop, dapl_set_prop, dapl_get_prop, dapl_delete_prop, dapl_copy_prop, dapl_compare_prop, dapl_close_prop );
}

hid_t dapl_create_class(  ){
	hid_t clazz = H5Pcreate_class( 0, H5CPP_DATASET_CUSTOM_PROP,
			dapl_class_create_func,nullptr, dapl_class_copy_func,nullptr, dapl_class_close_func,nullptr);
	dapl_register_prop( clazz );
	return clazz;
}

int main(int argc, char **argv) {

	/* SETUP: create a file and a dataset*/
	hsize_t dims[1] = {100};
        int buf[100] = {0,1,2,3,4,5,6,7,8,9,
                       10,11,12,13,14,15,16,17,18,19,
                       20,21,22,23,24,25,26,27,28,29,
                       30,31,32,33,34,35,36,37,38,39,
                       40,41,42,43,44,45,46,47,48,49,
                       50,51,52,53,54,55,56,57,58,59,
                       60,61,62,63,64,65,66,67,68,69,
                       70,71,72,73,74,75,76,77,78,79,
                       80,81,82,83,84,85,86,87,88,89,
                       90,91,92,93,94,95,96,97,98,99};
	// mock custom property with POD struct 
	custom_prop_t prop;

	std::cout<<"registerering custom properties with H5P_DATASET_CREATE and H5P_DATASET_ACCESS property classes:  \n";
	herr_t err_dcpl = H5Pregister2( H5P_DATASET_CREATE, H5CPP_DATASET_CUSTOM_PROP, sizeof(custom_prop_t), &prop,
			dapl_create_prop, dapl_set_prop, dapl_get_prop, dapl_delete_prop, dapl_copy_prop, dapl_compare_prop, dapl_close_prop );
	herr_t err_dapl = H5Pregister2( H5P_DATASET_ACCESS, H5CPP_DATASET_CUSTOM_PROP, sizeof(custom_prop_t), &prop,
			dapl_create_prop, dapl_set_prop, dapl_get_prop, dapl_delete_prop, dapl_copy_prop, dapl_compare_prop, dapl_close_prop );
	std::cout << "dcpl : " << err_dcpl << " dapl: " << err_dapl << " (expecting 0, no errror)\n";


	std::cout << "creating dcpl and dapl from respective property classes...\n";
	hid_t dcpl = H5Pcreate( H5P_DATASET_CREATE );
	hid_t dapl = H5Pcreate( H5P_DATASET_ACCESS );

	std::cout << "is custom prop present in passed " 
		<< " dcpl: " << H5Pexist(dcpl, H5CPP_DATASET_CUSTOM_PROP )
		<< " dapl: " << H5Pexist(dapl, H5CPP_DATASET_CUSTOM_PROP )
		<< " expecting 1 if yes\n";

	std::cout <<"creating HDF5 dataset with custom dapl and dcpl passed as arguments\n";
    hid_t file = H5Fcreate("custom_prop_test.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    hid_t space = H5Screate_simple (1, dims, NULL);
  	hid_t dset = H5Dcreate(file, "dataset", H5T_NATIVE_INT, space, H5P_DEFAULT, dcpl, dapl);

	std::cout << "obtaining dcpl and dapl from newly created dataset descriptor dset...\n";
	hid_t dcpl_dset = H5Dget_create_plist( dset );
	hid_t dapl_dset = H5Dget_access_plist( dset );
	std::cout << "verifying if custom properties present:\n"
		<< " dcpl :"	<< H5Pexist(dcpl_dset, H5CPP_DATASET_CUSTOM_PROP )
		<< " dapl :"	<< H5Pexist(dapl_dset, H5CPP_DATASET_CUSTOM_PROP ) <<"\n";

	std::cout << "writing some data into dataset\n";
    H5Dwrite(dset, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);

	std::cout << " re-openeing it with H5Dopen( ..., dapl)\n";
	H5Dclose(dset); H5Pclose(dcpl_dset); H5Pclose(dapl_dset);

   	dset = H5Dopen(file, "dataset", dapl);

	std::cout << "obtaining dcpl and dapl from re-opened dataset...\n";
	dcpl_dset = H5Dget_create_plist( dset );
	dapl_dset = H5Dget_access_plist( dset );


	std::cout << "verifying if custom properties present:\n"
		<< " dcpl :"	<< H5Pexist(dcpl_dset, H5CPP_DATASET_CUSTOM_PROP )
		<< " dapl :"	<< H5Pexist(dapl_dset, H5CPP_DATASET_CUSTOM_PROP ) <<"\n";

	std::cout <<"closing resources...\n";
	H5Pclose(dcpl_dset); H5Pclose(dapl_dset);
	H5Sclose(space);H5Dclose(dset);

	H5Fclose(file);
	return 0;
}

// SEE: 
// H5Dint.c line#1281: if(H5D__open_oid(dataset, dapl_id) < 0)
//
