#!/usr/bin/env python3

import jinja2
import yaml
import sys
import os

type_size_mapping = {
    "bool": 1,
    "uint8_t": 1,
    "uint16_t": 2,
    "uint32_t": 4,
    "int8_t": 1,
    "int16_t": 2,
    "int32_t": 4,
    "std::string": 32,  # Variable
}


class Parameter:
    "A parameter"

    def __init__(self, name, type):
        self.name = name
        self.type = type
        self.is_atomic = type not in ["std::string"]

    def __dict__(self):
        return {"name": self.name, "type": self.type, "is_atomic": self.is_atomic}


class ApplicationStateParameter:
    "A parameter in the application state"

    def __init__(self, name, parameter, default_value):
        self.index = -1
        self.name = name
        self.parameter = parameter
        self.default_value = default_value

        if isinstance(self.default_value, bool):
            self.default_value = "true" if self.default_value else "false"

        if not self.parameter.is_atomic:
            escape = '"' if self.parameter.type == "std::string" else ""
            self.default_value = f"{escape}{self.default_value}{escape}"

        if parameter.type == "std::string" and not isinstance(default_value, str):
            raise ValueError(f"Default value for {name} must be a string")

    def set_index(self, index):
        self.index = index

    def __dict__(self):
        return {
            "index": self.index,
            "name": self.name,
            "parameter": self.parameter.__dict__(),
            "default_value": self.default_value,
        }


def generate_output(
    template_directory, output_directory, parameters, application_state_parameters
):
    assert (
        len(application_state_parameters) > 0
    ), "No application state parameters defined"

    context = {
        "parameters": [param.__dict__() for param in parameters.values()],
        "application_state_parameters": [
            asp.__dict__() for asp in application_state_parameters
        ],
        "max_index": max(asp.index for asp in application_state_parameters),
    }

    template_loader = jinja2.FileSystemLoader(searchpath=template_directory)
    template_env = jinja2.Environment(loader=template_loader)

    hh_template = template_env.get_template("generated_application_state.hh.jinja2")
    hh_output = hh_template.render(
        parameters=context["parameters"],
        application_state_parameters=context["application_state_parameters"],
        max_index=context["max_index"],
    )
    with open(
        os.path.join(output_directory, "generated_application_state.hh"), "w"
    ) as f:
        f.write(hh_output)

    cc_template = template_env.get_template("generated_application_state.cc.jinja2")
    cc_output = cc_template.render(
        parameters=context["parameters"],
        application_state_parameters=context["application_state_parameters"],
    )
    with open(
        os.path.join(output_directory, "generated_application_state.cc"), "w"
    ) as f:
        f.write(cc_output)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(
            "Usage: generate_application_state.py <output_directory> <input_yaml> [input_yaml ...]"
        )
        sys.exit(1)

    template_directory = os.path.dirname(os.path.abspath(__file__))
    output_directory = sys.argv[1]
    input_files = sys.argv[2:]

    os.makedirs(output_directory, exist_ok=True)

    parameters = {}
    application_state_raw = []

    for input_file in input_files:
        with open(input_file, "r") as f:
            data = yaml.safe_load(f)

        if "parameters" in data:
            if not isinstance(data["parameters"], dict):
                print(f"Invalid parameters setup in {input_file}: expected mapping")
                sys.exit(1)

            for name, param in data["parameters"].items():
                if not isinstance(param, dict) or "type" not in param:
                    print(f"Invalid parameter setup in {input_file}: {name}")
                    sys.exit(1)
                parameters[name] = Parameter(name, param["type"])

        if "application_state" in data:
            if not isinstance(data["application_state"], dict):
                print(
                    f"Invalid application_state setup in {input_file}: expected mapping"
                )
                sys.exit(1)

            for name, default_value in data["application_state"].items():
                application_state_raw.append((name, default_value))

    # Resolve the application state
    application_state = []
    for name, default_value in application_state_raw:
        if name not in parameters:
            print(f"Unknown parameter in application_state: {name}")
            sys.exit(1)
        application_state.append(
            ApplicationStateParameter(name, parameters[name], default_value)
        )

    # Sort the application state by alignment (smallest first)
    application_state.sort(
        key=lambda asp: type_size_mapping[asp.parameter.type], reverse=False
    )

    for i, asp in enumerate(application_state):
        asp.set_index(i)

    generate_output(template_directory, output_directory, parameters, application_state)
