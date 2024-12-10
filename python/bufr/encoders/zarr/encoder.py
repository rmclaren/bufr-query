
import os
import re
from typing import Union

import zarr
import numpy as np

import bufr


# Encoder for Zarr format
class Encoder:
    def __init__(self, description: Union[str, bufr.encoders.Description]):
        if isinstance(description, str):
            self.description = bufr.encoders.Description(description)
        else:
            self.description = description

    def encode(self, container: bufr.DataContainer, output_path: str) -> dict[tuple[str],zarr.Group]:
        result:dict[tuple[str], zarr.Group] = {}
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

            result[tuple(category)] = root

        return result

    def _add_datasets(self, root:zarr.Group, container: bufr.DataContainer,
                      dims:dict, category:[str]):
        for var in self.description.get_variables():
            if var["source"] not in container.list():
                raise ValueError(f'Variable {var["source"]} not found in the container')

            data = container.get(var['source'], category)

            chunks = var['chunks'] if 'chunks' in var else None
            comp_level = var['compressionLevel'] if 'compressionLevel' in var else 3

            # Create the zarr dataset
            store = root.create_dataset(var['name'],
                                        shape=data.shape,
                                        chunks=chunks,
                                        dtype=data.dtype,
                                        compression='blosc',
                                        compression_opts=dict(cname='lz4',
                                                              clevel=comp_level,
                                                              shuffle=1))
            store[:] = data

            # Add the attributes
            store.attrs['units'] = var['units']
            store.attrs['longName'] = var['longName']

            if 'range' in var:
                store.attrs['valid_range'] = var['range']

            # Associate the dimensions
            store.attrs['_ARRAY_DIMENSIONS'] = dims[var["source"]]

    def _add_globals(self, root:zarr.Group):
        # Adds globals as attributes to the root group
        for key, data in self.description.get_globals().items():
            root.attrs[key] = data

    def _add_dimensions(self, root:zarr.Group, container: bufr.DataContainer, category:[str]):
        dims = {}

        named_dim_paths = {}
        named_dim_sources = {}
        named_dim_vars = {}

        # Add the location dimension
        named_dim_paths['Location'] = ['*']
        named_dim_vars['Location'] = container.list()[0]

        # Find the named dimensions
        for conf_dim in self.description.get_dims():
            named_dim_paths[conf_dim['name']] = conf_dim['paths']
            if conf_dim['source']:
                named_dim_sources[conf_dim['name']] = conf_dim['source']

        def find_named_dim(dim_path:str) -> str:
            for (key, path_list) in named_dim_paths.items():
                for path in path_list:
                    if path == dim_path:
                        return key
            return ''

        unamed_dim_idx = 1
        for var_name in container.list():
            if not var_name in dims:
                dims[var_name] = []

            for path in container.get_paths(var_name, category):
                dim_name = find_named_dim(path)
                if not dim_name:
                    dim_name = f'dim_{unamed_dim_idx}'
                    unamed_dim_idx += 1
                    named_dim_paths[dim_name] = [path]
                    named_dim_vars[dim_name] = var_name
                else:
                    named_dim_vars[dim_name] = var_name

                dims[var_name].append(f'/dimensions/{dim_name}')

        # # Create the datasets backing the dimensions
        dim_group = root.create_group('dimensions')
        for dim_name in named_dim_paths.keys():
            dim_paths = container.get_paths(named_dim_vars[dim_name], category)
            if dim_name in named_dim_sources:
                dim_data = container.get(named_dim_sources[dim_name], category)

                # Get the data associated with the last dimension of the source array
                dim_data = dim_data[(0,) * (dim_data.ndim - 1) + (slice(None),)]
            else:
                length = container.get(named_dim_vars[dim_name], category).shape[len(dim_paths) - 1]
                dim_data = np.arange(0, length)

            dim_store = dim_group.create_dataset(dim_name,
                                                 shape=dim_data.shape,
                                                 dtype=dim_data.dtype)
            dim_store[:] = dim_data

        return dims


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


