
import os
import re
from typing import Union

import zarr
import numpy as np

import bufr


# Encoder for Zarr format
class Encoder(bufr.encoders.EncoderBase):
    def __init__(self, description: Union[str, bufr.encoders.Description]):
        if isinstance(description, str):
            self.description = bufr.encoders.Description(description)
        else:
            self.description = description

        super(Encoder, self).__init__(self.description)

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

            if 'coordinates' in var:
                store.attrs['coordinates'] = var['coordinates']

            if 'range' in var:
                store.attrs['valid_range'] = var['range']

            # Associate the dimensions
            store.attrs['_ARRAY_DIMENSIONS'] = dims[var["source"]]

    def _add_globals(self, root:zarr.Group):
        # Adds globals as attributes to the root group
        for key, data in self.description.get_globals().items():
            root.attrs[key] = data

    def _add_dimensions(self, root:zarr.Group,
                        container: bufr.DataContainer,
                        category:[str]) -> dict[str, list[str]]:

        # Get the dimensions for the encoder
        dims = self.get_encoder_dimensions(container, category)

        # Add the backing variables for the dimensions
        dim_group = root.create_group('dimensions')
        for dim in dims.dims():
            dim_data = dim.labels
            dim_store = dim_group.create_dataset(dim, shape=[len(dim_data)], dtype=int)
            dim_store[:] = dim_data

        # Map the dimensions to the container variables
        var_dim_dict:dict[str, list[str]] = {}   # {var_name: [dim_name]}
        for var_name in container.list():
            var_dim_dict[var_name] = []
            for path in container.get_paths(var_name, category):
                var_dim_dict[var_name].append(self.find_named_dim_for_path(dims, path).name())

        return var_dim_dict

    def _make_path(self, prototype_path:str, sub_dict:dict[str, str]):
        subs = re.findall(r'\{(?P<sub>\w+\/\w+)\}', prototype_path)
        for sub in subs:
            prototype_path = prototype_path.replace(f'{{{sub}}}', sub_dict[sub])

        return prototype_path

    def _split_source_str(self, source:str) -> (str, str):
        components = source.split('/')
        group_name = components[0]
        variable_name = components[1]

        return (group_name, variable_name)

