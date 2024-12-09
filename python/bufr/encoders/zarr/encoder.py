
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

            for var in self.description.get_variables():
                if var["source"] not in container.list():
                    raise ValueError(f'Variable {var["source"]} not found in the container')

                self._add_dataset(container, var, root, category)

            # Close the zarr file
            root.store.close()

    def _add_dataset(self, container: bufr.DataContainer, var, root:zarr.Group, category:[str]):
        data = container.get(var['source'], category)

        # Create the zarr dataset
        store = root.create_dataset(var['name'], shape=data.shape, dtype=data.dtype)
        store[:] = data

        # Add the attributes
        store.attrs['units'] = var['units']
        store.attrs['longName'] = var['longName']

    def _add_globals(self, root:zarr.Group):
        for key, data in self.description.get_globals().items():
            if isinstance(data, list):
                store = root.create_dataset(key, shape=(len(data)), dtype=np.dtype(type(data[0])))
                store[:] = data
                print(root[key][:])
            # elif isinstance(data, str):
            #     store = root.create_dataset(key, shape=(), dtype=np.dtype(type(data)))
            #     store[()] = data
            #     print(root[key])
            # else:
            #     store = root.create_dataset(key, shape=(len(data)), dtype=np.dtype(type(data)))
            #     store[:] = data
            #     print(root[key][:])


            # print(root[key])
            # store = root.create_dataset(key, shape=data.shape, dtype=data.dtype)
            # store[:] = data

    def _add_dimensions(self, container: bufr.DataContainer, var, root:zarr.Group, category:[str]):
        data = container.get(var['source'], category)
        dimensions = container.get_paths(var['source'])

        for dim in dimensions:
            dim_data = container.get(dim, category)
            dim_store = root.create_dataset(dim, shape=dim_data.shape, dtype=dim_data.dtype)
            dim_store[:] = dim_data

            root[dim] = dim_store

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


