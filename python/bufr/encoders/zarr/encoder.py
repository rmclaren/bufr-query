
import os
import re
from typing import Union

import zarr
import numpy as np

import bufr
from numpy.ma.core import shape


# Encoder for Zarr format
class Encoder:
    def __init__(self, description: Union[str, bufr.encoders.Description]):
        if isinstance(description, str):
            self.description = bufr.encoders.Description(description)
        else:
            self.description = description

    def encode(self, container: bufr.DataContainer, output_path: str):
        for category in container.all_sub_categories():
            cat_idx = 0
            substitutions = {}
            for key in container.get_category_map().keys():
                substitutions[key] = category[cat_idx]
                cat_idx += 1

            root = zarr.open(self._make_path(output_path, substitutions), mode='w')
            self._add_globals(root)
            dims = self._add_dimensions(root, container, category)
            self._add_datasets(root, container, dims, category)

            # Close the zarr file
            root.store.close()

    def _add_datasets(self, root:zarr.Group, container: bufr.DataContainer,
                      dims:dict, category:[str]):
        for var in self.description.get_variables():
            if var["source"] not in container.list():
                raise ValueError(f'Variable {var["source"]} not found in the container')

            data = container.get(var['source'], category)

            # Create the zarr dataset
            store = root.create_dataset(var['name'], shape=data.shape, dtype=data.dtype)
            store[:] = data

            # Add the attributes
            store.attrs['units'] = var['units']
            store.attrs['longName'] = var['longName']

            # Associate the dimensions
            store.attrs['_ARRAY_DIMENSIONS'] = dims[var["source"]]

    def _add_globals(self, root:zarr.Group):
        # Adds globals as attributes to the root group
        for key, data in self.description.get_globals().items():
            root.attrs[key] = data

    def _add_dimensions(self, root:zarr.Group, container: bufr.DataContainer, category:[str]):
        dims = {}
        named_dims = {}

        # class Dim:
        #     def __init__(self, size:int, paths:[str], source:str=''):
        #         self.size = size
        #         self.paths = paths
        #         self.source = source


        named_dims['Location'] = ['*']
        location_length = container.get(container.list()[0], category).shape[0]
        store = root.create_dataset('Location', shape=(location_length,), dtype=np.int32)
        store[:] = np.arange(location_length)


        # Find the named dimensions
        for conf_dim in self.description.get_dims():
            named_dims[conf_dim['name']] = conf_dim['paths']
            if conf_dim['source']:
                print(conf_dim['source'])
                container.get(conf_dim['source'], category)
            else:
                pass

        def find_named_dim(dim_path:str) -> str:
            for (key, path_list) in named_dims.items():
                for path in path_list:
                    if path == dim_path:
                        return key
            return ''

        unamed_dim_idx = 1
        for var_name in container.list():
            if not var_name in dims:
                dims[var_name] = []

            data = container.get(var_name, category)
            for path in container.get_paths(var_name, category):
                dim_name = find_named_dim(path)
                if not dim_name:
                    dim_name = f'dim_{unamed_dim_idx}'
                    unamed_dim_idx += 1
                    named_dims[dim_name] = [path]

                dims[var_name].append(f'/{dim_name}')

        # # Create the datasets backing the dimensions
        # for dim_name in named_dims.keys():

        print(f'{dims}')

        return dims

        # data = container.get(var['source'], category)
        # dimensions = container.get_paths(var['source'])
        #
        # for dim in dimensions:
        #     dim_data = container.get(dim, category)
        #     dim_store = root.create_dataset(dim, shape=dim_data.shape, dtype=dim_data.dtype)
        #     dim_store[:] = dim_data
        #
        #     root[dim] = dim_store

    def _make_path(self, prototype_path:str, sub_dict:dict[str, str]):
        path_elements = os.path.split(prototype_path)
        filename = path_elements[1]

        subs = re.findall(r'\{(?P<sub>\w+\/\w+)\}', filename)

        for sub in subs:
            filename.replace(f'{{{sub}}}', sub_dict[sub])

        return os.path.join(path_elements[0], filename)


    def _split_source_str(self, source:str) -> (str, str):
        components = source.split('/')
        group_name = components[0]
        variable_name = components[1]

        return (group_name, variable_name)


